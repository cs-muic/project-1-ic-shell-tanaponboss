#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
// #include <unistd.h>
// #include<sys/wait.h>
#include <signal.h>
#include <stddef.h>
#include<fcntl.h>



#define MAX_CMD_BUFFER 255
#define MAX_CMD_WORD 255

int pid;
int status;
int execute(char **args);
char **divideCmd(char *args);
int runShell(FILE *file);
void sig_suspend();
void sig_kill();

int runShell(FILE *file) {
    char buffer[MAX_CMD_BUFFER];
    bool exit = false;
    char **cmds;
    char cmd_hist[MAX_CMD_BUFFER] = "";

    while (!exit) {

        signal(SIGTSTP, sig_suspend);
        signal(SIGINT, sig_kill);

        if (file == stdin) {
            printf("icsh $ ");
            fflush(stdout);
        }

        if (!fgets(buffer, sizeof(buffer), file))
            break;

        // Remove trailing newline character from the buffer
        buffer[strcspn(buffer, "\n")] = '\0';

        // If cmd buffer = !! and cmd_history arr is empty, print "prev cmd not available"
        if (strcmp(buffer, "!!") == 0 && cmd_hist[0] == '\0') {
            printf("prev cmd not available\n");
        }
        // If input buffer is not empty, keep it in cmd_hist
        else if (strcmp(buffer, "!!") != 0) {
            strcpy(cmd_hist, buffer);
        }

        // Check if the line starts with "##" (comment) or is empty
        if (buffer[0] != '#' && buffer[1] != '#') {
            cmds = divideCmd(buffer);
            if (cmds != NULL) {
                //check exit
                if (strcmp(cmds[0], "exit") == 0) {
                    printf("Adios\n");
                    int exitCode = 0;
                    if (cmds[1] != NULL) {
                        exitCode = atoi(cmds[1]);
                    }
                    exit = true;
                    return exitCode;
                } else if (strcmp(cmds[0], "!!") == 0 && strcmp(cmd_hist, "!!") != 0) {
                    char **prevCommand = divideCmd(cmd_hist);
                    exit = execute(prevCommand);
                } else {
                    exit = execute(cmds);
                }
            }
        }
    }

    fclose(file);

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc == 2) {
        FILE *file = fopen(argv[1], "r");
        if (file == NULL) {
            printf("Error opening file: %s\n", argv[1]);
            return 1;
        }
        int exitCode = runShell(file);
        fclose(file);
        return exitCode;
    } else {
        int exitCode = runShell(stdin);
        return exitCode;
    }
}

int execute(char **cmds) {

    if (cmds[0] == NULL) {
        return false;
    } else if (strcmp(cmds[0], "echo") == 0) {
        printf("%s", cmds[1]);
        for (int i = 2; cmds[i] != NULL; i++) {
            printf(" %s", cmds[i]);
        }
        printf("\n");
        return false;
    } else {
       	if ((pid = fork())< 0){
            perror("Fork failed");
            exit(1);
            
        }
    //If fork succeeds, the child process calls the execvp function kub to execute the external program 
    //specified by the first cmd in the cmds array, passing the rest of the cmds as arguments to the program. 
    //If execvp fails, it prints an error message and exits.
        if(!pid){
            status = execvp(cmds[0], cmds);
            if (status == -1){
                printf("Invalid Cmd\n");
            }
            exit(1);
        }
    //The parent process waits for the child process to complete using the waitpid function. 
    //Once the child process has completed, the function returns false so that the shell keeps runnig kub.    
        if(pid){
            waitpid(pid, NULL, 0);
            
        }
        return false;
    }
    return false;
}

char **divideCmd(char *args) {
    if (args[0] == '\0') {
        return NULL;
    }
    int index = 0;
    char *cmd;
    char **cmds = malloc(MAX_CMD_WORD * sizeof(char *));
    cmd = strtok(args, " ");
    while (cmd != NULL) {
        cmds[index] = cmd;
        index++;
        if (index >= (MAX_CMD_WORD)) {
            break;
        }
        cmd = strtok(NULL, " ");
    }
    return cmds;
}

void sig_suspend(){
    kill(pid, SIGTSTP);
    printf("Process Suspended\n");
    runShell(stdin);
}

void sig_kill(){
    kill(pid, SIGINT);
    printf("Process Killed\n");
}