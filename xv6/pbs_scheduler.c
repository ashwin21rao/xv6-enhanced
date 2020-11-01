#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
# include "traps.h"
#include "spinlock.h"

// referenced from proc.c
struct {
    struct spinlock lock;
    struct proc proc[NPROC];
} ptable;


// trap handler
void
trap(struct trapframe *tf)
{
    if(tf->trapno == T_SYSCALL){
        if(myproc()->killed)
            exit();
        myproc()->tf = tf;
        syscall();
        if(myproc()->killed)
            exit();
        return;
    }

    switch(tf->trapno){
        case T_IRQ0 + IRQ_TIMER:
            if(cpuid() == 0){
                acquire(&tickslock);
                ticks++;
                // Update run time or iotime of running process which got interrupted by timer
                if(myproc())
                {
                    if(myproc()->state == RUNNING)
                        myproc()->rtime++;
                    else if(myproc()->state == SLEEPING)
                        myproc()->iotime++;
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
        case T_IRQ0 + IRQ_IDE+1:
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
            if(myproc() == 0 || (tf->cs&3) == 0){
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
    if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
        exit();

    // Force process to give up CPU on clock tick.
    // If interrupts were on while locks held, would need to check nlock.
    if(myproc() && myproc()->state == RUNNING &&
       tf->trapno == T_IRQ0+IRQ_TIMER)
        yield();

    // Check if the process has been killed since we yielded
    if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
        exit();
}

// helper function which returns minimum value of priority (highest priority)
int
getminpriority()
{
    struct proc *p;
    int min_priority_value = 150;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if (p->state != RUNNABLE)
            continue;
        if(p->priority < min_priority_value)
            min_priority_value = p->priority;
    }
    return min_priority_value;
}

//  PRIORITY BASED (PBS) scheduler
void
scheduler(void)
{
    struct proc *p;
    struct cpu *c = mycpu();
    c->proc = 0;
    int min_priority_value;

    for(;;){
        // Enable interrupts on this processor.
        sti();

        acquire(&ptable.lock);
        min_priority_value = getminpriority();
        if(min_priority_value == 150)
        {
            release(&ptable.lock);
            continue; // no process is runnable, hence continue looking
        }

        // round robin between equal priorities
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
            if (p->state != RUNNABLE)
                continue;
            if(p->priority == min_priority_value)
            {
                // Switch to chosen process.  It is the process's job
                // to release ptable.lock and then reacquire it
                // before jumping back to us.
                c->proc = p;
                switchuvm(p);
                p->state = RUNNING;
                p->n_run++;
//                cprintf("PBS: Switching to process %d\n", p->pid);
                swtch(&(c->scheduler), p->context);
                switchkvm();

                // Process is done running for now.
                // It should have changed its p->state before coming back.
                c->proc = 0;

                if(getminpriority() < min_priority_value) // preempt round robin for lower priority process
                    break;
            }
        }
        release(&ptable.lock);
    }
}
