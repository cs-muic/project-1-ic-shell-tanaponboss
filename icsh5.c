#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_CMD_BUFFER 255
#define MAX_CMD_WORD 255

int pid;
int status;
int execute(char **args);
char **divideCmd(char *args);
int runShell(FILE *file);
void sig_suspend(int signal);
void sig_kill(int signal);
pid_t foreground_pgid = 0;  // Stores the foreground process group ID
bool shouldExit = false;

int runShell(FILE *file) {
    char buffer[MAX_CMD_BUFFER];
    char **cmds;
    char cmd_hist[MAX_CMD_BUFFER] = "";

    while (!shouldExit) {
        if (file == stdin) {
            printf("icsh $ ");
            fflush(stdout);
        }

        signal(SIGTSTP, SIG_IGN);  
        signal(SIGINT, SIG_IGN);   

        if (!fgets(buffer, sizeof(buffer), file))
            break;

        // Remove trailing newline character from the buffer i.e. remove \n when enter is the input
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
                int exitCode = checkExit(cmds);
                if (exitCode != -1 ) {
                    return exitCode;
                }
                if (strcmp(cmds[0], "!!") == 0 && strcmp(cmd_hist, "!!") != 0) {
                    char **prevCommand = divideCmd(cmd_hist);
                    shouldExit = execute(prevCommand);
                } else {
                    shouldExit = execute(cmds);
                }
            }
        }
    }

    fclose(file);

    return 0;
}

int main(int argc, char *argv[]) {

    if (argc >= 2) {
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
        pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        } else if (pid == 0) {
            signal(SIGTSTP, SIG_DFL);  // Set SIGTSTP to default in the child process
            signal(SIGINT, SIG_DFL);   // Set SIGINT to default in the child process

            execvp(cmds[0], cmds);
            printf("Invalid Cmd\n");
            exit(1);
        } else {
            // Parent process
            signal(SIGTSTP, SIG_IGN);  // Ignore SIGTSTP in the parent process
            signal(SIGINT, SIG_IGN);   // Ignore SIGINT in the parent process

            foreground_pgid = pid; // Update the foreground process group ID

            int childStatus;
            // Wait for the child process to complete or suspend
            waitpid(pid, &childStatus, WUNTRACED);  

            if (WIFSTOPPED(childStatus)) {
                // Child process was suspended
                printf("Process Suspended\n");
            }

            foreground_pgid = 0; // Reset the foreground process group ID

            return false;
        }
    }
}

int checkExit(char **cmds) {
    if (strcmp(cmds[0], "exit") == 0) {
        printf("Adios\n");
        int exitCode = atoi(cmds[1]);
        shouldExit = true;
    if (exitCode > 255) {
       return exitCode >> 8;
    }
        else return exitCode;
    }
    else {return -1;}
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
        if (index >= MAX_CMD_WORD) {
            break;
        }
        cmd = strtok(NULL, " ");
    }
    cmds[index] = NULL;  // Add a NULL terminator at the end of the command list
    return cmds;
}

void sig_suspend(int signal) {
    if (foreground_pgid != 0) {
        kill(-foreground_pgid, SIGTSTP);  // Suspend the foreground process group
        printf("Process Suspended\n");
    }
}

void sig_kill(int signal) {
    if (foreground_pgid != 0) {
        kill(-foreground_pgid, SIGINT);  // Terminate the foreground process group
        printf("Process Killed\n");
    }
}

int outIndex(char** arr){
    for (int tt = 0; arr[tt] != NULL; tt++) {
        if (strcmp(arr[tt], ">") == 0) {
            return tt;
        }
    }
    return -1;
}

int inIndex(char** arr){
    for (int bb = 0; arr[bb] != NULL; bb++) {
        if (strcmp(arr[bb], "<") == 0) {
            return bb;
        }
    }
    return -1;
}

int IORedirection(char** list){

    char* in;
    char* out;
    FILE* rfp;
    FILE* wfp;
    int input, output;

    input = inIndex(list);
    output = outIndex(list);


    if (input >= 0) {

        in = list[input + 1];
        list[input] = NULL;
        rfp = freopen(in, "r", stdin);

    }

    if (output >= 0) {

        out = list[output + 1];
        list[output] = NULL;
        wfp = freopen(out, "w", stdout);

    }
}

