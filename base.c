#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_PATH 256
#define MAX_NAME 64


struct Process {
    int pid;         
    int ppid;           
    char state;        
    char cmd[MAX_NAME]; 
};
int parse_proc_stat(int pid, struct Process *proc) {
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "/proc/%d/stat", pid);
    FILE *file = fopen(path, "r");
    if (!file) return 0; 
    char buffer[512];
    if (fgets(buffer, sizeof(buffer), file)) {
        char cmd[MAX_NAME];
        int parsed_pid, parsed_ppid;
        char state;
        if (sscanf(buffer, "%d (%[^)]) %c %d", &parsed_pid, cmd, &state, &parsed_ppid) == 4) {
            proc->pid = parsed_pid;
            proc->ppid = parsed_ppid;
            proc->state = state;
            strncpy(proc->cmd, cmd, MAX_NAME - 1);
            proc->cmd[MAX_NAME - 1] = '\0';
            fclose(file);
            return 1; 
        }
    }
    fclose(file);
    return 0; 
}
int main() {
    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) {
        perror("Failed to open /proc");
        return 1;
    }

    printf("Scanning for zombie processes...\n");
    printf("%-10s %-10s %-10s %s\n", "PID", "PPID", "State", "Command");

    struct dirent *entry;
    struct Process proc;
    int zombie_count = 0;

    while ((entry = readdir(proc_dir))) {
        if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
            int pid = atoi(entry->d_name);
            if (parse_proc_stat(pid, &proc) && proc.state == 'Z') {
                zombie_count++;
                printf("%-10d %-10d %-10c %s\n", proc.pid, proc.ppid, proc.state, proc.cmd);
                
                char response[10];
                printf("Kill parent (PID %d) of zombie (PID %d)? (y/n): ", proc.ppid, proc.pid);
                if (fgets(response, sizeof(response), stdin) && response[0] == 'y') {
                    if (kill(proc.ppid, SIGTERM) == 0) {
                        printf("Sent SIGTERM to parent PID %d\n", proc.ppid);
                    } else {
                        perror("Failed to send SIGTERM");
                    }
                }
            }
        }
    }

    closedir(proc_dir);
    if (zombie_count == 0) {
        printf("No zombie processes found.\n");
    } else {
        printf("Found %d zombie processes.\n", zombie_count);
    }

    return 0;
}
