#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_CMD_BUFFER 255
#define MAX_CMD_WORD 255

int execute(char **args);
char **divideCmd(char *args);

int runShell(FILE *file) {
    char buffer[MAX_CMD_BUFFER];
    bool exit = false;
    char **cmds;
    char cmd_hist[MAX_CMD_BUFFER] = "";

    while (!exit) {
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

int execute(char **args) {
    if (args[0] == NULL) {
        return false;
    } else if (strcmp(args[0], "echo") == 0) {
        printf("%s", args[1]);
        for (int i = 2; args[i] != NULL; i++) {
            printf(" %s", args[i]);
        }
        printf("\n");
        return false;
    } else {
        printf("Invalid Command\n");
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
