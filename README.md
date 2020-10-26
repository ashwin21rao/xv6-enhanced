## INTRODUCTION

This project builds upon the [xv6 operating system](https://github.com/mit-pdos/xv6-public) developed by MIT.  

The following additional features have been implemented:
- System calls: ```waitx```, ```procinfo```
- User programs (invoked from command line): ```time```, ```ps```, ```setPriority```

There is a choice between the following 4 CPU scheduling algorithms:
- Round Robin (RR) - default scheduler provided by the original xv6 operating system.
- First Come First Served (FCFS)
- Priority Based Scheduling (PBS)
- Multi Level Feedback Queue (MLFQ)


## BUILDING AND RUNNING xv6

- Install the QEMU PC simulator.
- Run ```make qemu SCHEDULER=<scheduler type>``` where ```scheduler type``` is one of ```RR```, ```FCFS```, ```PBS``` 
  and ```MLFQ```. If not specified, the Round Robin scheduler is used.
- For more details look at the [original README](README_original).


## ADDED USER COMMANDS

1. ```time <command>```
   - Prints the total run time of the command and the total time it spent waiting for a CPU.
   - Uses the ```waitx``` system call.
   - Ex: ```time ls```

2. ```ps```
   - Prints details of all processes in the system.

3. ```setPriority <new_priority> <pid>```
   - For a priority based scheduler, sets the priority of the process with process id ```pid``` to ```new_priority```. 


## IMPLEMENTATION OF ADDITIONAL FEATURES

### ```waitx``` system call
  
- The variable ```rtime``` stores the total runtime of the process. This is incremented after each clock tick the 
  process runs (on timer interrupt).
- The total waiting time of the process is calculated using the formula:  
  ```total_wait_time = end_time - total_run_time - creation_time```
  
### ```ps``` user program

- This makes use of the ```procinfo``` system call which prints out the details of every process in the operating 
  system, obtained directly from the process structure.
- ```n_run``` is incremented just before a process starts running on a CPU.
- ```w_time``` is the amount of time the process has been waiting:
  - For a CPU to run on, if the process is runnable.
  - For it to be woken up, if the process is sleeping.
- ```wtime``` is calculated using ```w_time = ticks - q_toe``` where ```ticks``` is the current time and ```ustime``` is the most recent time the 
  process state was changed to ```RUNNABLE``` or ```SLEEPING```. 

### ```FCFS``` scheduler

- The process with the oldest creation time is found by looping over all processes and is run to completion or until it 
  blocks. 
- There is no preemption. This is done by preventing the process from calling ```yield()``` on timer interrupt,
  which causes it to relinquish control of the CPU.
  
### ```PBS``` scheduler

- The highest priority in the system is found by looping over all processes.
- All processes with this priority are scheduled in a Round Robin manner.
- After a process yields control of the CPU on timer interrupt, the highest priority in the system is found again. 
  - If this priority is the same as before, we continue the Round Robin scheduler. 
  - Else, we begin scheduling all processes with the new highest priority by Round Robin.
  
### ```MLFQ``` scheduler

- The priority of the current queue which the process is in is stored in ```cur_q```. There are no actual queues 
  in the implementation.
- The time of entry of a process into a queue is stored in ```q_toe```. Processes with the smallest value of 
  ```q_toe``` will be at the head of the queue.
- We loop over the 5 priorities from highest to lowest (0-4).
- For each priority, we select the process at the head of the queue and run it for a time slice of ```1 << priority``` 
  ticks. This value is stored in ```q_ticks``` of the process and is decremented after every timer interrupt. Once 
  the value of ```q_ticks``` becomes ```0```, it relinquishes control of the CPU.
- If the value of ```q_ticks``` after the process yields the CPU is ```0```, it means that the process used up its 
  entire time slice. Else, the process either exited or blocked before its time slice completed.
- A process which finished its time slice in a queue (except the lowest queue) is moved down to the end of the next 
  queue by incrementing  ```cur_q``` and resetting ```q_toe``` to be equal to ```ticks```. 
- For a process which did not use up its entire time slice, ```q_toe``` is reset to ```ticks``` when the process 
  becomes runnable again, hence moving it to the end of the same queue once it wakes up.
- For a process in the lowest queue which used up its time slice, ```cur_q``` remains the same and ```q_toe``` is reset.
  Hence Round Robin is automatically implemented for the lowest queue.
- After a process uses up its time slice, we loop over all processes in the system and implement aging. If a process
  has waited for the CPU for more than ```5 * time slice``` ticks, it is moved to the end of the previous queue 
  (with higher priority) by decrementing ```cur_q``` and resetting ```q_toe``` to ```ticks```.
- If at least one process aged, we start looping again from the head of the highest priority (```0```), to account for
  possible new entries in higher priority queues. Else, we schedule the next process at the head of the current queue.
- Once the current queue from which we are scheduling processes becomes empty, we begin scheduling processes from the 
  head of the next (lower priority) queue.
