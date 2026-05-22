#include <stdio.h>       
#include <string.h>      
#include <stdlib.h>      
#include <unistd.h>      
#include <sys/wait.h>   
#include <linux/limits.h> 
#include "LineParser.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>


int debug_mode = 0;

void excecute(cmdLine *pCmdLine){
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return;
    }
    if (pid == 0) { // child: apply redirections first, then replace with the command

        if (pCmdLine->inputRedirect != NULL) {
        int inFd = open(pCmdLine->inputRedirect, O_RDONLY);
        if (inFd < 0) {
            perror("failed to redirect input file");
            _exit(1);
        }
        if (dup2(inFd, STDIN_FILENO) < 0) {
            perror("failed to redirect stdin");
            close(inFd);
            _exit(1);
        }
        close(inFd);
        }
        
       if (pCmdLine->outputRedirect != NULL) {
            int outFd = open(pCmdLine->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (outFd < 0) {
                perror("failed to redirect output file");
                _exit(1);
            }
            if (dup2(outFd, STDOUT_FILENO) < 0) {
               perror("failed to redirect stdout");
               close(outFd);
               _exit(1);
            }
            close(outFd);
        }        

        if(execvp(pCmdLine->arguments[0], pCmdLine->arguments) == -1){
            perror("excecution failed");
            _exit(1);
        }
    } else { // parent: wait for the child to finish execution or continue to the next command
        if (debug_mode) {
            fprintf(stderr, "PID: %d\n", pid);
            fprintf(stderr, "Executing command: %s\n", pCmdLine->arguments[0]);
            fprintf(stderr, "Mode: %s\n", pCmdLine->blocking ? "foreground" : "background");
        }
        sleep(1);
        if (pCmdLine->blocking) {
            waitpid(pid, NULL, 0); 
        }
    }
}

/* return value:
    1  -  no signal was needed to be sent.
    0  -  signal was sent.
   -1  -  tried to send a signal and failed. */
int handleSignal(cmdLine *pCmdLine) {
    if (pCmdLine -> argCount != 2) {
        return 1;
    }
    char* cmd = pCmdLine->arguments[0];
    char* targetPIDstr = pCmdLine->arguments[1];
    int ret = 0;
    int signalNum = 0;
    int targetPID = atoi(targetPIDstr); 

    if (strcmp(cmd, "stop") == 0) {
        signalNum = SIGSTOP;
    } else if (strcmp(cmd, "wakeup") == 0) {
        signalNum = SIGCONT;
    } else if (strcmp(cmd, "ice") == 0) {
        signalNum = SIGINT;
    } else if (strcmp(cmd, "nuke") == 0) {
        signalNum = SIGKILL;
        targetPID = -targetPID; 
    } else {
        return 1;
    }

    if (targetPID == 0) { 
        fprintf(stderr, "Invalid PID: %s\n", targetPIDstr);
        return -1;
    }

    ret = kill(targetPID, signalNum);
    sleep(1);

    if (ret == -1) {
        fprintf(stderr, "Tried to send %s to %d and failed: %s\n", cmd, targetPID, strerror(errno));
    } else if (debug_mode == 1) {
        fprintf(stdout, "Sent %s to %d \n", cmd, targetPID);
    }

    return ret;
}

int main(int argc, char **argv){

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            debug_mode = 1;
        }
    }

    char input[2048];
    char cwd[PATH_MAX];

    while(1){

        if(getcwd(cwd, PATH_MAX) != NULL){
            printf("%s$ ", cwd);
        }


        if (fgets(input, 2048, stdin) == NULL) {  //checking for EOF
                break;
        }

        cmdLine *parsedLine = parseCmdLines(input);
        if (parsedLine == NULL) {
            continue;
        }

        if (strcmp(parsedLine->arguments[0], "quit") == 0) {
            freeCmdLines(parsedLine);
            break;
        }


        if (strcmp(parsedLine->arguments[0], "cd") == 0) {
            if (chdir(parsedLine->arguments[1]) < 0) {
                perror("cd failed");
            }
            freeCmdLines(parsedLine);
            continue;
        }
    

        if(handleSignal(parsedLine) == 1){
            excecute(parsedLine);
        }
        freeCmdLines(parsedLine);
    }

    return 0;
}

