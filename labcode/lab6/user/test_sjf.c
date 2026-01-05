#include <stdio.h>
#include <ulib.h>

#define MAX_CHILDREN 5

int main(void) {
    int i, pid;
    cprintf("SJF Test Started\n");

    // We want to test if processes with smaller priority (burst time) run first.
    // We create children with decreasing priorities: 5, 4, 3, 2, 1.
    // If SJF works, they should finish in reverse order: Child 4 (prio 1), Child 3 (prio 2), ...
    
    for (i = 0; i < MAX_CHILDREN; i++) {
        if ((pid = fork()) == 0) {
            // Child process
            // Set priority. In our SJF implementation, priority is treated as burst time.
            // Smaller priority = shorter burst time = runs first.
            uint32_t priority = MAX_CHILDREN - i; 
            lab6_setpriority(priority);
            cprintf("Child %d created with priority %d\n", i, (int)priority);
            
            // Yield to ensure we are in the run queue with the new priority
            // and to give other children a chance to be created and set their priorities.
            yield();
            
            // When we get the CPU back, it means we were picked by the scheduler.
            cprintf("Child %d executing\n", i);
            
            // Simulate work
            int j;
            for (j = 0; j < 10000000; j++); 
            
            cprintf("Child %d finished\n", i);
            exit(0);
        }
        cprintf("Parent forked child %d\n", i);
    }

    cprintf("Parent waiting for children...\n");
    // Parent waits. This might block the parent, allowing children to run.
    for (i = 0; i < MAX_CHILDREN; i++) {
        wait();
    }
    cprintf("SJF Test Finished\n");
    return 0;
}
