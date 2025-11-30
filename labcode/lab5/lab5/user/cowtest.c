/*
 * COW (Copy-on-Write) Test Program
 * 
 * This test verifies the correct implementation of COW mechanism.
 */

#include <ulib.h>
#include <stdio.h>

// Global variable to test COW on data segment
int global_var = 100;

int main(void)
{
    cprintf("========================================\n");
    cprintf("    COW (Copy-on-Write) Test Suite     \n");
    cprintf("========================================\n");

    cprintf("\n=== Test: Basic COW Test ===\n");

    cprintf("Before fork: global_var = %d\n", global_var);

    int pid = fork();

    if (pid == 0)
    {
        // Child process
        cprintf("Child: Before write - global_var = %d\n", global_var);

        // Write to trigger COW
        global_var = 200;

        cprintf("Child: After write - global_var = %d\n", global_var);

        if (global_var == 200)
        {
            cprintf("Child: COW write test PASSED\n");
        }
        else
        {
            cprintf("Child: COW write test FAILED\n");
        }

        exit(0);
    }
    else if (pid > 0)
    {
        // Parent process - wait for child
        int exit_code;
        wait();

        cprintf("Parent: After child exit - global_var = %d\n", global_var);

        // Verify parent's value is unchanged
        if (global_var == 100)
        {
            cprintf("Parent: COW isolation test PASSED\n");
        }
        else
        {
            cprintf("Parent: COW isolation test FAILED\n");
        }
    }
    else
    {
        cprintf("Fork failed!\n");
    }

    cprintf("\n========================================\n");
    cprintf("    COW Test Completed!                \n");
    cprintf("========================================\n");

    return 0;
}