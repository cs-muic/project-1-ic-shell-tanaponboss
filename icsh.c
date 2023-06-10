#include <signal.h>
#include <stddef.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/fcntl.h>

#define MAX_CMD_BUFFER 255
#define MAX_CMD_WORD 255


# define N_CHAR 256
# define N_PID 4194304
# define N_JOBS 1000

struct job {
    char command[N_CHAR];
    int pid;
    int job_id;
    int status; //0=fg,1=bg,2=stopped
};

char prev_command[N_CHAR] = {'\0'};  

char shell_comment[] = "##";
// int fg_pid = 0;  // Stores the foreground process group ID
pid_t fg_pid = 0;  // Stores the foreground process group ID
int prev_exit_status = 0;
int saved_stdout;
int current_nb = 0; //current number of background job

pid_t ppid;
pid_t jobs_to_pid[N_JOBS]; //map job IDs to their corresponding process IDs (PIDs)
//For eg, if the job ID is 1, the PID will be stored at jobs_to_pid[1]

pid_t jobs_order[2];
struct job pids_command[N_PID];

// int pid;
// int status;
char **divideCmd(char *args);
int runShell(FILE *file);
void sig_suspend(int signal);
void sig_kill(int signal);
int inPos(char **args);
int outPos(char **args);
int IORedirection(char **list);
bool shouldExit = false;

void swap_jobs_order(int nb) {
    jobs_order[1] = jobs_order[0];
    jobs_order[0] = nb;
}

char *get_job_status(int status) {
    switch (status){
        case 1:
            return "Running";
        case 2:
            return "Stopped";
        default:
            return "";
    }
}


char get_jobs_sign(pid_t job_id) {
    if(jobs_order[0] == job_id) 
        return '+';
    else if(jobs_order[1] == job_id) 
        return '-';
    else
        return ' ';
}

void process_fg(char* cmd) {
    int job_id = parse_amp_job(cmd);
    pid_t pid = jobs_to_pid[job_id];
    if(pid) {
        struct job* job_ = &pids_command[pid];
        job_->status = 0;
        printf("%s\n", job_->command);
        swap_jobs_order(job_id);
        kill(pid, SIGCONT);
        int status;
        setpgid(pid, pid);
        tcsetpgrp (0, pid);
        waitpid(pid, &status, 0);
        waitpid(pid, &status, 0);
        tcsetpgrp (0, ppid);
        prev_exit_status = WEXITSTATUS(status);
    } 
    else if (job_id)
        printf("fg: %%%d: no such job\n", job_id);
}

void process_bg(char *cmd) {
    int job_id = parse_amp_job(cmd);
    pid_t pid = jobs_to_pid[job_id];
    if(pid) {
        struct job* job_ = &pids_command[pid];
        char sign = get_jobs_sign(job_->job_id);
        printf("[%d]%c %s &\n",  job_->job_id, sign, job_->command);
        job_->status = 1;
        kill(pid, SIGCONT);
    }
    else if (job_id > 0){
        printf("bg: %%%d: no such job\n", job_id);
    }
}

int is_bg(char cmds[]){
    //1) Check if cmds is not empty (cmds)
    //2) Check if cmds is not a null terminator (*cmds)
    //null terminator is '\0' which is used to mark the end of a string.
    //3) Check the '&' at the end which means it's a bg progress
    // printf("in is_bg with cmds = %s \n", cmds);
    if(cmds && *cmds && cmds[strlen(cmds) - 1] == '&') {
        cmds[strlen(cmds) - 1] = '\0';
        char *end;
        end = cmds + strlen(cmds) - 1;
        while(end > cmds && isspace((unsigned char)*end))
            end--;
        end[1] = '\0';
        return 1;
    }
    return 0;
}

void get_jobs() {
    for(int i = 1; i <= current_nb; i++) {
        pid_t pid = jobs_to_pid[i];
        if(pid != 0) {
            struct job* job_ = &pids_command[pid];
            if(job_ != NULL && job_->status > 0) {
                char sign = get_jobs_sign(i);
                printf("[%d]%c  %s      %s &\n", 
                i, sign, get_job_status(job_->status), job_->command);
            }
        }
    }
}

void child_handler (int sig, siginfo_t *sip, void *notused){
    int status = 0;
    if (sip->si_pid == waitpid (sip->si_pid, &status, WNOHANG | WUNTRACED)){
        if (WIFEXITED(status)|| WTERMSIG(status)) {
            struct job* job_ = &pids_command[sip->si_pid];
            if(WIFEXITED(status) && job_ != NULL && job_->status == 1 && job_->job_id) {
                char sign = get_jobs_sign(job_->job_id);
                printf("\n[%d]%c  Done                   %s\n",  job_->job_id, sign, job_->command);
                fflush (stdout); 
                jobs_to_pid[job_->job_id] = 0;
                struct job null_job;
                null_job.job_id = 0;
                null_job.pid = 0;
                pids_command[sip->si_pid] = null_job;
                runShell(stdin);
            }
            else if (WIFSTOPPED(status) && job_ != NULL) {
                if(!(job_->job_id)) {
                    job_->job_id = ++current_nb;
                    jobs_to_pid[job_->job_id] = sip->si_pid;
                    swap_jobs_order(job_->job_id);
                } 
                job_->status = 2;
                char sign = get_jobs_sign(job_->job_id);
                printf("\n[%d]%c  Stopped                    %s\n",  job_->job_id, sign, job_->command);
                fflush (stdout); 
            }
        }
    } 
}
void fg_handler() {

}

void stop_handler() {

}

void init_sig() {

    //Ctrl ^C
    struct sigaction sigint, prevSigint;
    sigint.sa_handler = fg_handler;
    sigint.sa_flags = 0; //no action initially
    sigemptyset(&sigint.sa_mask); //clear signal mask ensures that no signals are blocked when sigint handler functions are executed.
    sigaction(SIGINT, &sigint, &prevSigint); //register signal and store old signal in prevSigint

    //Ctrl ^Z
    struct sigaction sigstp, prevSigstp;
    sigstp.sa_handler = stop_handler;
    sigstp.sa_flags = 0; //no action initially
    sigemptyset(&sigstp.sa_mask);  //clear signal mask ensures that no signals are blocked when sigstp handler functions are executed.
    sigaction(SIGTSTP, &sigstp, &prevSigstp); //register signal and store old signal in prevSigint

    //To prevent background processes from being stopped by signals.
    signal(SIGTTOU, SIG_IGN); //SIGTTOU (terminal output for background process)
    signal(SIGTTIN, SIG_IGN); //SIGTTIN (terminal input for background process)

    struct sigaction action;
    action.sa_sigaction = child_handler; //child_handler is the fn that will be called when a child process terminates.

    sigfillset (&action.sa_mask); //sets the signal mask for action to include all signals which mean that all signals will be blocked while the child_handler function is executed.
    action.sa_flags = SA_SIGINFO;
    sigaction (SIGCHLD, &action, NULL); //registers SIGCHLD signal, which is sent when a child process 
    //terminates. The NULL argument becuz the previous signal handler is not stored.
}


int runShell(FILE *file) {
    char buffer[MAX_CMD_BUFFER];
    char **cmds;
    char cmd_hist[MAX_CMD_BUFFER] = "";

    while (!shouldExit) {

        //using memset ensures that the buffer array is initialized with null characters (0) 
        //before reading any data into it
        memset(buffer,0,MAX_CMD_BUFFER); 

        if (file == stdin) {
            printf("icsh $ ");
            fflush(stdout);
        }

        signal(SIGTSTP, sig_suspend);  // Set SIGTSTP to default in the child process
        signal(SIGINT, sig_kill);
        
        //checks if any error occurred or if the end of the file was reached. If so, break.
        if (!fgets(buffer, sizeof(buffer), file))
            break;

        // Remove trailing newline character '\n' from the buffer
        buffer[strcspn(buffer, "\n")] = '\0';
        char* originCmd[MAX_CMD_BUFFER];
        strcpy(originCmd, buffer);
        // printf("bufer in runshell 1: %s \n", originCmd);


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
            // printf("buffer in run shell %s \n", buffer);
            cmds = divideCmd(buffer);

            if (cmds != NULL) {
                // printf("Parsed command: %s\n", cmds[0]); // Debug print
                int exitCode = checkExit(cmds);
                //handle exit
                if (exitCode != -1 ) {
                    return exitCode;
                }
                //handle !!
                if (strcmp(cmds[0], "!!") == 0 && strcmp(cmd_hist, "!!") != 0) {
                    char **prevCommand = divideCmd(cmd_hist);
                    shouldExit = execute(prevCommand, originCmd);
                } else {
                    shouldExit = execute(cmds, originCmd);
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

int parse_amp_job(char* cmd) {
    if(cmd == NULL || cmd[0] != '%') {
        printf("Invalid arguments: %s\n", cmd);
        return 0;
    }
    const char t[2] = "%";
    char* tmp;
    tmp = strtok(cmd, t);
    return atoi(tmp);
}

int execute(char** cmds, char buffer[]) {
    char *cmd;
    char *tmp = (char *) malloc(strlen(buffer) + 1);
    // printf("buffer in exe %s \n", buffer);

    int bgp = is_bg(buffer);
    // printf("%s is bg = %d \n", buffer, bgp);
    strcpy(tmp, buffer);
    cmd = strtok(tmp, " ");

    if(cmd != NULL) {
        if (strcmp(cmds[0], "") == 0) {
            return false;
        } else if (strcmp(cmds[0], "echo") == 0) {
            printf("%s", cmds[1]);
            for (int i = 2; cmds[i] != NULL; i++) {
                printf(" %s", cmds[i]);
            }
            printf("\n");
            return false;
        } else if (!strcmp(cmds[0], "jobs")) {
            get_jobs();
        }else if(!strcmp(cmds[0], "fg")) {
            cmd = strtok(NULL, " ");
            process_fg(buffer);
        } 
        else if(!strcmp(cmds[0], "bg")) {
            cmd = strtok(NULL, " ");
            process_bg(buffer);
        }
        else {
            int status;
            int pid;
            
            pid = fork();
            if (pid < 0) {
                perror("Fork failed");
                exit(1);
            } else if (pid == 0) {
                // signal(SIGTSTP, sig_suspend);  // Set SIGTSTP to default in the child process
                // signal(SIGINT, sig_kill);   // Set SIGINT to default in the child process
                setpgid(0, 0);
                int input = inPos(cmds);
                int output = outPos(cmds);
                // printf("outPos returned: %d\n", output); // Debug print


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
                    // printf("Output file: %s\n", filename); // Debug print
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
                // execvp(cmds[0], cmds);
                // printf("Invalid Cmd\n");
                // exit(1);
                int i = 0;
                char * prog_arv[N_CHAR];
                cmd = strtok(buffer, " ");
                prog_arv[i] = cmd;
                while(cmd != NULL) {
                    cmd = strtok(NULL, " ");
                    prog_arv[++i] = cmd;
                }
                prog_arv[i+1] = NULL;
                execvp(prog_arv[0], prog_arv);
                // exit(errno);
            } else if (pid) {
                // Parent process
                // printf("enters parent process with buffer %s\n", buffer);
                struct job curr_job;
                strcpy(curr_job.command , buffer);
                curr_job.pid = pid;
                curr_job.status = bgp;
                curr_job.job_id = bgp ? ++current_nb : 0;
                pids_command[pid] = curr_job;
                setpgid(pid, pid);

                if(!bgp) {
                    tcsetpgrp (0, pid);
                    waitpid(pid, &status, 0);
                    tcsetpgrp (0, ppid);
                    prev_exit_status = WEXITSTATUS(status);
                } 
                else {
                    jobs_to_pid[current_nb] = pid;
                    // printf("switch to bg process");
                    swap_jobs_order(current_nb);
                    printf("[%d] %d\n", current_nb, pid);
                }
                return false;
            }

        }
        if(strcmp(prev_command, buffer)) 
            strcpy(prev_command, buffer);
        // return false;    
    }
    free(tmp);    
    return false;
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



int main(int argc, char *argv[]) {

    ppid = getpid(); //generate PID from unistd.h lib
    init_sig(); //initiate signal handler
    char command[N_CHAR];

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


void sig_suspend(int signal) {
    if (fg_pid != 0) {
        kill(-fg_pid, SIGTSTP);  // Suspend the foreground process group
        printf("Process Suspended\n");
    }
    runShell(stdin);
}

void sig_kill(int signal) {
    if (fg_pid != 0) {
        kill(-fg_pid, SIGINT);  // Terminate the foreground process group
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


