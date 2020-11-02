## INTRODUCTION

This project builds upon the [xv6 operating system](https://github.com/mit-pdos/xv6-public) developed by MIT.  
Look at the [project report](REPORT.md) for implementation details of additional features and comparison of performance 
of different scheduling algorithms.

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
- ```cd``` into the xv6 folder.
- Run ```make qemu SCHEDULER=<scheduler type>``` where ```scheduler type``` is one of ```RR```, ```FCFS```, ```PBS``` 
  and ```MLFQ```. If not specified, the Round Robin scheduler is used.
- For more details look at the [original README](xv6/README).


## ADDED USER COMMANDS

1. ```time <command>```
   - Prints the total run time of the command and the total time it spent waiting for a CPU.
   - Uses the ```waitx``` system call.
   - Ex: ```time ls```

2. ```ps```
   - Prints details of all processes in the system.

3. ```setPriority <new_priority> <pid>```
   - For a priority based scheduler, sets the priority of the process with process id ```pid``` to ```new_priority```. 
