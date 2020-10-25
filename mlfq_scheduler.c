#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// referenced from proc.c
struct {
    struct spinlock lock;
    struct proc proc[NPROC];
} ptable;


// trap handler
void
trap(struct trapframe *tf) {
    if (tf->trapno == T_SYSCALL) {
        if (myproc()->killed)
            exit();
        myproc()->tf = tf;
        syscall();
        if (myproc()->killed)
            exit();
        return;
    }

    switch (tf->trapno) {
        case T_IRQ0 + IRQ_TIMER:
            if (cpuid() == 0) {
                acquire(&tickslock);
                ticks++;
                // Update run time and queue ticks of running process which got interrupted by timer
                if (myproc() && myproc()->state == RUNNING) {
                    myproc()->rtime += 1;
                    if (myproc()->cur_q == 0)
                        myproc()->q0++;
                    else if (myproc()->cur_q == 1)
                        myproc()->q1++;
                    else if (myproc()->cur_q == 2)
                        myproc()->q2++;
                    else if (myproc()->cur_q == 3)
                        myproc()->q3++;
                    else if (myproc()->cur_q == 4)
                        myproc()->q4++;
                }
                wakeup(&ticks);
                release(&tickslock);
            }
            lapiceoi();
            break;
        case T_IRQ0 + IRQ_IDE:
            ideintr();
            lapiceoi();
            break;
        case T_IRQ0 + IRQ_IDE + 1:
            // Bochs generates spurious IDE1 interrupts.
            break;
        case T_IRQ0 + IRQ_KBD:
            kbdintr();
            lapiceoi();
            break;
        case T_IRQ0 + IRQ_COM1:
            uartintr();
            lapiceoi();
            break;
        case T_IRQ0 + 7:
        case T_IRQ0 + IRQ_SPURIOUS:
            cprintf("cpu%d: spurious interrupt at %x:%x\n",
                    cpuid(), tf->cs, tf->eip);
            lapiceoi();
            break;

            //PAGEBREAK: 13
        default:
            if (myproc() == 0 || (tf->cs & 3) == 0) {
                // In kernel, it must be our mistake.
                cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
                        tf->trapno, cpuid(), tf->eip, rcr2());
                panic("trap");
            }
            // In user space, assume process misbehaved.
            cprintf("pid %d %s: trap %d err %d on cpu %d "
                    "eip 0x%x addr 0x%x--kill proc\n",
                    myproc()->pid, myproc()->name, tf->trapno,
                    tf->err, cpuid(), tf->eip, rcr2());
            myproc()->killed = 1;
    }

    // Force process exit if it has been killed and is in user space.
    // (If it is still executing in the kernel, let it keep running
    // until it gets to the regular system call return.)
    if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
        exit();

    // Force process to give up CPU on clock tick.
    // If interrupts were on while locks held, would need to check nlock.
    if (myproc() && myproc()->state == RUNNING &&
        tf->trapno == T_IRQ0 + IRQ_TIMER)
        yield();

    // Check if the process has been killed since we yielded
    if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
        exit();
}

// helper function to get process at head of queue
//struct proc *
//getQueueSize(int q_num)
//{
//    struct proc *sp = 0;
//    int min_q_toe = 2e9;
//    for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
//        if(p->state != RUNNABLE)
//            continue;
//        if (p->cur_q == q_num) {
//            //cprintf("Found one process in queue %d with pid %d\n", pr->cur_q, pr->pid);
//            if (p->q_toe < min_q_toe) {
//                min_q_toe = p->q_toe;
//                sp = p;
//            }
//        }
//    }
//    return sp;
//}

void
scheduler(void) {
    struct proc *sp; // process which will be scheduled
    struct cpu *c = mycpu();
    c->proc = 0;
    int min_q_toe; // minimum time of entry (head of that queue)

    for (;;) {
        // Enable interrupts on this processor.
        sti();

        acquire(&ptable.lock);
        // Loop over queues from highest to lowest priority
        for (int i = 0; i <= 4; i++) {
            min_q_toe = 2e9;
            sp = 0;
            for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
                if(p->state != RUNNABLE)
                    continue;
                if (p->cur_q == i) {
                    //cprintf("Found one process in queue %d with pid %d\n", pr->cur_q, pr->pid);
                    if (p->q_toe < min_q_toe) {
                        min_q_toe = p->q_toe;
                        sp = p;
                    }
                }
            }
            // if no runnable process in the current priority queue, move down one level
            if (sp == 0) {
                continue;
            }

            int q_ticks = 1 << i; // number of ticks in time slice in queue i (1, 2, 4, 8, 16)

            // run chosen process for entire time slice or until it finishes
            for (int r = 0; r < q_ticks; r++) {
                // process is done running
                if(sp->state != RUNNABLE)
                    break;

                // Switch to chosen process.  It is the process's job
                // to release ptable.lock and then reacquire it
                // before jumping back to us.
                c->proc = sp;
                switchuvm(sp);
                sp->state = RUNNING;
                cprintf("MLFQ: Switching to process %d in queue %d\n", sp->pid, sp->cur_q);
                swtch(&(c->scheduler), sp->context);
                //cprintf("MLFQ: Back to scd!\n");
                switchkvm();

                // Process is done running for now.
                // It should have changed its p->state before coming back.
                c->proc = 0;
            }

            // after process finishes time slice, move down one level
            if (sp->cur_q != 4)
                sp->cur_q++;
            // update time of entry into new queue
            sp->q_toe = ticks;

            // aging of process: move up one level if it waits for too long in a queue
            int aged = 0;
            for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
                if((ticks - p->q_toe) >  5 * q_ticks){
                    if(p->cur_q > 0)
                        p->cur_q--;
                    p->q_toe = ticks;
                    aged = 1;
                }
            }

            // start from beginning (queue 0) to account for aged processes
            if(aged == 1)
                i = -1;
        }
        release(&ptable.lock);
    }
}
