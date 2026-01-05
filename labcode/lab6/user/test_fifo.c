#include <stdio.h>
#include <ulib.h>

#define MAX_CHILDREN 5

int main(void) {
    int i, pid;
    cprintf("FIFO Test Started\n");

    for (i = 0; i < MAX_CHILDREN; i++) {
        if ((pid = fork()) == 0) {
            // Child process
            cprintf("Child %d running\n", i);
            // Do some work to simulate CPU usage
            int j;
            // Increase loop count to make it noticeable
            for (j = 0; j < 10000000; j++); 
            cprintf("Child %d finished\n", i);
            exit(0);
        }
        cprintf("Parent forked child %d\n", i);
    }

    cprintf("Parent waiting for children...\n");
    for (i = 0; i < MAX_CHILDREN; i++) {
        wait();
    }
    cprintf("FIFO Test Finished\n");
    return 0;
}
