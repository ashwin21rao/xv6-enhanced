#include "types.h"
#include "user.h"

int number_of_processes = 40;

int main(int argc, char *argv[]) {
    for (volatile int i = 0; i < number_of_processes; i++) {
        int pid = fork();
        if (pid < 0) {
            printf(1, "Fork failed\n");
            continue;
        }
        if (pid == 0) {
            if (i % 4 == 0) {
                // CPU
                for (volatile int j = 0; j < 2 * 1e8; j++) {
                    ;
                }
            }
            else if (i % 4 == 1) {
                // IO
                for(volatile int j = 0; j < 10; j++){
                    sleep(70);
                }
            }
            else if (i % 4 == 2) {
                // IO then CPU
                sleep(500);
                for(volatile int k = 0; k < 2 * 1e8; k++){
                    ;
                }
            }
            else if (i % 4 == 3) {
                // CPU then IO
                for(volatile int k = 0; k < 2 * 1e8; k++){
                    ;
                }
                sleep(500);
            }
//            printf(2, "Benchmark: %d Exited (category: %d)\n", i, i % 4);
            exit();
        } else {
            set_priority(100 - (20 + i) % 2, pid); // will only matter for PBS, comment it out if not implemented yet (better priority for more IO intensive jobs)
        }
    }

    for (int j = 0; j < number_of_processes + 5; j++) {
        wait();
    }
    exit();
}
