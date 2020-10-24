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
                // Update run time of running process which got interrupted by timer
                if(myproc() && myproc()->state == RUNNING)
                    myproc()->rtime += 1;
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


//PAGEBREAK: 42
// Per-CPU process scheduler (ROUND ROBIN).
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
    struct proc *p;
    struct cpu *c = mycpu();
    c->proc = 0;
    struct proc *priority_process; // process with highest priority (minimum value)
    int min_priority_value;

    for(;;){
        // Enable interrupts on this processor.
        sti();

        acquire(&ptable.lock);
        min_priority_value = 150;
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
            if (p->state != RUNNABLE)
                continue;
            if(p->ctime < min_priority_value)
            {
                min_priority_value = p->priority;
                priority_process = p;
            }
        }
        if(min_priority_value == 150)
        {
            release(&ptable.lock);
            continue; // no process is runnable, hence continue looking
        }

        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        c->proc = priority_process;
        switchuvm(priority_process);
        priority_process->state = RUNNING;
        cprintf("PBS: Switching to process %d\n", priority_process->pid);
        swtch(&(c->scheduler), priority_process->context);
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        release(&ptable.lock);
    }
}
