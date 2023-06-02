/* ICCS227: Project 1: icsh
 * Name: Tanapon
 * StudentID: 
 */

#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h> 

int execute(char **);
char ** divideCmd(char *);

#define MAX_CMD_BUFFER 255
#define MAX_CMD_WORD 255
int main() {
    char buffer[MAX_CMD_BUFFER];
    bool exit = false;
    char** cmds;
    char cmd_hist[MAX_CMD_BUFFER] = "";

    while (!exit) {
        printf("icsh $ ");
        fgets(buffer, 255, stdin);

        //if cmd buffer = !! then perform the prev cmd
        //compare if cmd buffer = !! and cmd_history arr is empty then print prev cmd not avialable      
        
        if (strcmp(buffer, "!!\n") == 0 && cmd_hist[0] == '\0'){
			printf("prev cmd not available \n");
		}
        // if input buffer not empty then keep in cmd_hist
        else if (strcmp(buffer, "!!\n") != 0){
			strcpy(cmd_hist, buffer);		
		}

        cmds = divideCmd(buffer);
        //exit
        if(strcmp(cmds[0], "exit") == 0){
			printf("Adios \n");
			int arg1 = atoi(cmds[1]);
			if (arg1 > 255){
				return arg1 >> 8;
			} else{
			return arg1;
			}
		}

		else if(strcmp(cmds[0], "!!\n") == 0 && strcmp(cmd_hist, "!!\n") != 0){

			char ** prevCommand = divideCmd(cmd_hist);
			exit = execute(prevCommand);
		}
		else{		
			exit = execute(cmds);
		}		
	}
	return 0;
}


int execute(char ** args){


	if (args == NULL){
		return false;
	}
	else if ( strcmp(args[0], "echo") == 0){
		printf("%s", args[1]);
		for(int i = 2; args[i] != NULL; i++){
			printf(" %s", args[i]);
		}
		
		return false;
	}
	else {
		printf("Invalid Command\n");
	}
	return false;

}

char ** divideCmd(char * args){

    if (args[0] == '\0') {
        return NULL;
    }
	
	int index = 0;
    char * cmd;
	char ** cmds = malloc(MAX_CMD_WORD * sizeof(char *));
    //strtok function to split the input string into single cmd separated by spaces.
	cmd = strtok(args, " ");
	while(cmd != NULL){
		cmds[index] = cmd;
		index ++;
        //The loop continues until either all cmds have been processed or the maximum number of cmds (MAX_CMD_WORD) has been reached.
		if(index >= (MAX_CMD_WORD)){
			break;
		}
        //sets cmd to the next cmd in the input string or to NULL if there are no more cmds. 
		cmd = strtok(NULL, " ");
	}
	
	return cmds;
}
