#include "types.h"
#include "stat.h"
#include "user.h"


int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        printf(2, "Not enough arguments\n");
        exit();
    }
    int pid = fork();
    if(pid == 0)
        exec(argv[1], argv + 1);
    else
    {
        int wtime, rtime;
        waitx(&wtime, &rtime);
        printf(1, "Waiting time of process: %d\n", wtime);
        printf(1, "Running time of process: %d\n", rtime);
    }
    exit();
}
