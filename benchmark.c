#include "types.h"
#include "user.h"

int number_of_processes = 50;

int main(int argc, char *argv[]) {
    for (int i = 0; i < number_of_processes; i++) {
        int pid = fork();
        if (pid < 0) {
            printf(1, "Fork failed\n");
            continue;
        }
        if (pid == 0) {
            int total = 0;

            if (i % 4 == 0) {
                // CPU
                for (int j = 0; j < 1e9; j++) {
                    total += j;
                }
            }
            else if (i % 4 == 1) {
                // IO
                for(int j = 0; j < 10; j++){
                    sleep(70);
                    total += j;
                }
            }
            else if (i % 4 == 2) {
                // IO then CPU
                sleep(500);
                for(int j = 0; j < 5; j++){
                    total += j;
                    for(int k = 0; k < 1e8; k++){
                        total += k;
                    }
                }
            }
            else if (i % 4 == 3) {
                // CPU then IO
                for(int j = 0; j < 5; j++){
                    total += j;
                    for(int k = 0; k < 1e8; k++){
                        total += k;
                    }
                }
                sleep(500);
            }
            printf(2, "Benchmark: %d Exited, Category : %d, Total : %d\n", i, i % 4, total);
            exit();
        } else {
            set_priority(100 - (20 + i) % 2, pid); // will only matter for PBS, comment it out if not implemented yet (better priorty for more IO intensive jobs)
        }
    }

    for (int j = 0; j < number_of_processes + 5; j++) {
        wait();
    }
    exit();
}