#include <signal.h>
#include <stddef.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/fcntl.h>

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
int inPos(char **args);
int outPos(char **args);
int IORedirection(char **list);
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

        // signal(SIGTSTP, SIG_IGN);
        // signal(SIGINT, SIG_IGN);

        signal(SIGTSTP, sig_suspend);  // Set SIGTSTP to default in the child process
        signal(SIGINT, sig_kill);
        
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
            strncpy(cmd_hist, buffer, sizeof(cmd_hist) - 1);
        //To ensures that the cmd_hist array is properly null-terminated, even if the buffer array was longer than cmd_hist.
            cmd_hist[sizeof(cmd_hist) - 1] = '\0';
        }

        // Check if the line starts with "##" (comment) or is empty
        if (buffer[0] != '#' && buffer[1] != '#') {
            cmds = divideCmd(buffer);

            if (cmds != NULL) {
                printf("Parsed command: %s\n", cmds[0]); // Debug print
                int exitCode = checkExit(cmds);
                //handle exit
                if (exitCode != -1 ) {
                    return exitCode;
                }
                //handle !!
                if (strcmp(cmds[0], "!!") == 0 && strcmp(cmd_hist, "!!") != 0) {
                    char **prevCommand = divideCmd(cmd_hist);
                    shouldExit = execute(prevCommand);
                } else {
                    shouldExit = execute(cmds);
                }
            }
            else {
                continue;
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
int execute(char** cmds) {
    if (strcmp(cmds[0], "") == 0) {
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
            // signal(SIGTSTP, sig_suspend);  // Set SIGTSTP to default in the child process
            // signal(SIGINT, sig_kill);   // Set SIGINT to default in the child process

            int input = inPos(cmds);
            int output = outPos(cmds);
            printf("outPos returned: %d\n", output); // Debug print


            if (input >= 0) {
                // Input redirection is requested
                char* filename = cmds[input + 1];
                cmds[input] = NULL;  // Remove the "<" operator and filename from the command

                // Open the file for reading
                int fd = open(filename, O_RDONLY);
                if (fd < 0) {
                    perror("Failed to open file for reading");
                    exit(1);
                }

                // Redirect stdin to the file
                if (dup2(fd, STDIN_FILENO) < 0) {
                    perror("Failed to redirect stdin to file");
                    exit(1);
                }

                // Close the file descriptor
                close(fd);
            }

            if (output >= 0) {
                // Output redirection is requested
                char* filename = cmds[output + 1];
                printf("Output file: %s\n", filename); // Debug print
                cmds[output] = NULL;  // Remove the ">" operator and filename from the command

                // Open the file for writing
                int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd < 0) {
                    perror("Failed to open file for writing");
                    exit(1);
                }

                // Redirect stdout to the file
                if (dup2(fd, STDOUT_FILENO) < 0) {
                    perror("Failed to redirect stdout to file");
                    exit(1);
                }

                // Close the file descriptor
                close(fd);
            }

            // Execute the command
            execvp(cmds[0], cmds);
            printf("Invalid Cmd\n");
            exit(1);
        } else {
            // Parent process
            signal(SIGTSTP, SIG_IGN);  // Ignore SIGTSTP in the parent process
            signal(SIGINT, SIG_IGN);   // Ignore SIGINT in the parent process

            foreground_pgid = pid;  // Update the foreground process group ID

            int childStatus;
            // Wait for the child process to complete or suspend
            waitpid(pid, &childStatus, WUNTRACED);

            if (WIFSTOPPED(childStatus)) {
                // Child process was suspended
                printf("Process Suspended\n");
            }

            foreground_pgid = 0;  // Reset the foreground process group ID

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
        // if (strcmp(cmd, ">") == 0 || strcmp(cmd, "<") == 0) {
        //     // Redirection operator found, skip adding it to the command list
        //     cmds[index] = NULL;
        // } else {
        //     cmds[index] = cmd;
        // }
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
    runShell(stdin);
}

void sig_kill(int signal) {
    if (foreground_pgid != 0) {
        kill(-foreground_pgid, SIGINT);  // Terminate the foreground process group
        printf("Process Killed\n");
    }
}

// find out position
int outPos(char **arr) {
    for (int i = 0; arr[i] != NULL; i++) {
        if (strcmp(arr[i], ">") == 0) {
            return i;
        }
    }
    return -1;
}

// find in position
int inPos(char **arr) {
    for (int i = 0; arr[i] != NULL; i++) {
        if (strcmp(arr[i], "<") == 0) {
            return i;
        }
    }
    return -1;
}

int IORedirection(char** list) {
    char* in = NULL;
    char* out = NULL;

    int input = inPos(list);
    int output = outPos(list);

    if (input >= 0) {
        in = list[input + 1];
        list[input] = NULL;
    }

    if (output >= 0) {
        out = list[output + 1];
        list[output] = NULL;
    }

    if (in != NULL) {
        // Input redirection is requested
        FILE* file = freopen(in, "r", stdin);
        if (file == NULL) {
            perror("Failed to open input file");
            return 1;
        }
    }

    if (out != NULL) {
        // Output redirection is requested
        FILE* file = freopen(out, "w", stdout);
        if (file == NULL) {
            perror("Failed to open output file");
            return 1;
        }
    }

    return 0;
}

