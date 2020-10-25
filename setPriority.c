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
    if(argc > 3)
    {
        printf(2, "Too many arguments\n");
        exit();
    }
    int old_priority = set_priority(atoi(argv[1]), atoi(argv[2]));
    printf(1, "DONE. OLD PRIORITY = %d\n", old_priority);
    exit();
}
