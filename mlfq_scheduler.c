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
        tf->trapno == T_IRQ0 + IRQ_TIMER) {

        // yield CPU only after completion of time slice of process
        myproc()->q_ticks--;
        if(myproc()->q_ticks == 0)
            yield();
        cprintf("Process %d in queue %d not yielding\n", myproc()->pid, myproc()->cur_q);
    }

    // Check if the process has been killed since we yielded
    if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
        exit();
}

// helper function to get process at head of queue
struct proc *
getFirstProcess(int q_num)
{
    struct proc *sp = 0;
    int min_q_toe = 2e9; // minimum time of entry (process at head of that queue)
    for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if(p->state != RUNNABLE)
            continue;
        if (p->cur_q == q_num) {
            if (p->q_toe < min_q_toe) {
                min_q_toe = p->q_toe;
                sp = p;
            }
        }
    }
    return sp;
}

void
scheduler(void) {
    struct proc *sp; // process which will be scheduled
    struct cpu *c = mycpu();
    c->proc = 0;

    for (;;) {
        // Enable interrupts on this processor.
        sti();

        acquire(&ptable.lock);
        // Loop over queues from highest to lowest priority
        for (int i = 0; i <= 4; i++) {
            sp = getFirstProcess(i); // get process at head of queue
            // if no runnable process in the current priority queue, move down one level
            if (sp == 0)
                continue;

            // number of ticks in time slice in queue i (1, 2, 4, 8, 16)
            sp->q_ticks= 1 << i;

            // Run chosen process for entire time slice or until it relinquishes CPU
            // Switch to chosen process.  It is the process's job
            // to release ptable.lock and then reacquire it
            // before jumping back to us.
            c->proc = sp;
            switchuvm(sp);
            sp->state = RUNNING;
            sp->n_run++;
            cprintf("MLFQ: Switching to process %d in queue %d\n", sp->pid, sp->cur_q);
            swtch(&(c->scheduler), sp->context);
            switchkvm();

            // Process is done running for now.
            // It should have changed its p->state before coming back.
            // p->q_toe is also updated before coming back
            c->proc = 0;

            // If process used entire time slice, move it down one level
            // If process exited, it is removed from queue system
            // If process blocked, it is pushed to end of the same queue once it wakes up
            if(sp->q_ticks == 0) {
                if(sp->state == ZOMBIE)
                    sp->cur_q = -1;
                else if (sp->cur_q != 4)
                    sp->cur_q++;
            }
            // update time of entry into new queue (or same queue) - this is taken care of before control returns to scheduler
//            sp->q_toe = ticks;

            // aging of processes: move process up one level if it waits for too long in a queue (5 * time slice)
            int aged = 0;
            for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
                if(p->state != RUNNABLE)
                    continue;
                if((ticks - p->q_toe) > 5 * (1 << p->cur_q)) {
                    if(p->cur_q > 0)
                    {
                        p->cur_q--;
                        p->q_toe = ticks;
                        aged = 1;
                        cprintf("Process %d aged from %d to %d\n", p->pid, p->cur_q+1, p->cur_q);
                    }
                }
            }

            if(aged == 1)
                i = -1; // start from beginning (queue 0) to account for aged processes
            else
                i--; // get next process at head of same queue
        }
        release(&ptable.lock);
    }
}
