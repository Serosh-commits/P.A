#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process - exit immediately to become zombie
        printf("Child process (PID: %d) exiting to become zombie\n", getpid());
        exit(0);
    } else if (pid > 0) {
        // Parent process - sleep without calling wait()
        printf("Parent process (PID: %d) created child (PID: %d)\n", getpid(), pid);
        printf("Child will become zombie. Parent sleeping for 30 seconds...\n");
        printf("Run './process_analyzer' and press 'Z' to see the zombie process\n");
        sleep(30);
        
        // Clean up the zombie
        wait(NULL);
        printf("Zombie cleaned up\n");
    } else {
        perror("fork failed");
        return 1;
    }
    
    return 0;
}