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

#define TERMINATED  -1
#define RUNNING 1
#define SUSPENDED 0

typedef struct process{
    cmdLine* cmd;
    pid_t pid;
    int status;
    struct process *next;
} process;

void updateProcessList(process **process_list);



/*-----------------------------------------task 3a-------------------------------------------------*/

void addProcess(process** process_list, cmdLine* cmd, pid_t pid) {
    struct process* newProcess = malloc(sizeof(process));
    newProcess -> cmd = cmd;
    newProcess -> pid = pid;
    newProcess -> status = 1;
    newProcess -> next = *process_list;
    *process_list = newProcess;
    
}	

const char* getStatusText(int status) {
    if (status == RUNNING) {
        return "Running";
    }
    if (status == SUSPENDED) {
        return "Suspended";
    }
    return "Terminated";
}

void printProcessList(process** process_list) {
    updateProcessList(process_list);
    process* curr = *process_list;
    while (curr) {
        printf("%d\t%s\t", curr -> pid, getStatusText(curr -> status));

        cmdLine* current_cmd = curr->cmd;
        while (current_cmd != NULL) {

            for (int i = 0; i < current_cmd->argCount; i++) {
                printf("%s", current_cmd->arguments[i]);
                if (i < current_cmd->argCount - 1) {
                    printf(" ");
                }
               
            }

            current_cmd = current_cmd->next;
            if (current_cmd != NULL) {
                printf(" | ");
            }
        }
        printf("\n"); 
        curr = curr -> next;

        }

    //delete dead processes
    curr = *process_list;
    process* first = NULL;
    process* prev = NULL;
    while (curr) {
        process* next = curr -> next;
        if(curr -> status == TERMINATED) {
            if (prev) {
                prev -> next = curr -> next;
            }
            freeCmdLines(curr -> cmd);
            free(curr);
        } else {
            prev = curr;
            if (!first) { first = curr; }
        }
        curr = next;
    }

    *process_list = first;
}

/*-----------------------------------------task 3b-------------------------------------------------*/

void freeProcessList(process* process_list) {
    process* tmp = NULL;
    while (process_list) {
        tmp = process_list -> next;
        freeCmdLines(process_list -> cmd);
        free(process_list);
        process_list = tmp;
    }
}
int get_process_status(pid_t pid, int current_status) {
    int wstatus;
    pid_t result = waitpid(pid, &wstatus, WNOHANG | WUNTRACED | WCONTINUED);
    if (result == 0)
        return current_status;
    if (result > 0){
        if (WIFSTOPPED(wstatus))  return SUSPENDED;  
        if (WIFCONTINUED(wstatus))  return RUNNING;
        if (WIFSIGNALED(wstatus) || WIFEXITED(wstatus)) return TERMINATED;
    }
    return TERMINATED; 
    
}

void updateProcessList(process **process_list) {
     if (process_list) {
        process* curr = *process_list;
        while(curr) {
            curr->status = get_process_status(curr->pid, curr->status);
            curr = curr->next;
        }
    } 
    
}

void updateProcessStatus(process* process_list, int pid, int status) {
    process* curr = process_list;
    while(curr) {
        if (pid == curr->pid) {
            curr->status = status;
        }
        curr = curr->next;
    }
}
/*-------------------------------------------------------------------------------------------------*/


process* global_process_list = NULL;
int debug_mode = 0;
int last_command_tracked = 0;

void execute_pipeline(cmdLine *left_cmd, process **proc_list) {
    cmdLine *right_cmd = left_cmd->next;

    if (left_cmd->outputRedirect) {
        fprintf(stderr, "Error: Left process cannot redirect stdout in a pipeline.\n");
        return;
    }
    if (right_cmd->inputRedirect) {
        fprintf(stderr, "Error: Right process cannot redirect stdin in a pipeline.\n");
        return;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        return;
    }

    pid_t child1 = fork();
    if (child1 == -1) { perror("fork failed"); close(pipefd[0]); close(pipefd[1]); return; }
    
    if (child1 == 0) {
        setpgid(0, 0); // create a process group for the pipeline
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        close(pipefd[0]);

        if (left_cmd->inputRedirect) {
            int in_fd = open(left_cmd->inputRedirect, O_RDONLY);
            if (in_fd >= 0) { dup2(in_fd, STDIN_FILENO); close(in_fd); }
        }

        execvp(left_cmd->arguments[0], left_cmd->arguments);
        perror("Left pipeline execution failed");
        _exit(1);
    }

    setpgid(child1, child1);

    pid_t child2 = fork();
    if (child2 == -1) { perror("fork failed"); close(pipefd[0]); close(pipefd[1]); return; }
    
    setpgid(child2, child1);

    if (child2 == 0) {
        setpgid(0, child1); // group second child under first child's process group

        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        if (right_cmd->outputRedirect) {
            int out_fd = open(right_cmd->outputRedirect, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out_fd >= 0) { dup2(out_fd, STDOUT_FILENO); close(out_fd); }
        }

        execvp(right_cmd->arguments[0], right_cmd->arguments);
        perror("Right pipeline execution failed");
        _exit(1);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    if (right_cmd->blocking) {
        waitpid(child1, NULL, 0);
        waitpid(child2, NULL, 0);
    }
    else{
        addProcess(proc_list, left_cmd, child1); //process group led by child1
        last_command_tracked = 1;
    }
}



void excecute(cmdLine *pCmdLine){
    if (!pCmdLine) {
        last_command_tracked = 0;
        return;
    }

    last_command_tracked = 0;

    if (pCmdLine->next != NULL) {
        execute_pipeline(pCmdLine, &global_process_list);
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return;
    }
    if (pid == 0) { // child: apply redirections first, then replace with the command
        setpgid(0, 0);
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
            return;
        }
        addProcess(&global_process_list, pCmdLine, pid);
        last_command_tracked = 1;
    }
}

/* return value:
    1  -  no signal was needed to be sent.
    0  -  signal was sent.
   -1  -  tried to send a signal and failed. */
int handleSignal(cmdLine *pCmdLine) {
    if (pCmdLine -> argCount == 2) {
        char* cmd = pCmdLine->arguments[0];
        char* targetPIDstr = pCmdLine->arguments[1];
        int ret = 0;
        int signalNum = 0;
        int requestedPID = atoi(targetPIDstr);
        int targetPID = atoi(targetPIDstr);
        int updatedStatus = RUNNING;

        if (strcmp(cmd, "stop") == 0) {
            signalNum = SIGSTOP;
            updatedStatus = SUSPENDED;
        } else if (strcmp(cmd, "wakeup") == 0) {
            signalNum = SIGCONT;
            updatedStatus = RUNNING;
        } else if (strcmp(cmd, "ice") == 0) {
            signalNum = SIGINT;
            updatedStatus = TERMINATED;
        } else if (strcmp(cmd, "nuke") == 0) {
            signalNum = SIGKILL;
            updatedStatus = TERMINATED;
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
            fprintf(stderr, "Tried to send %s to %d and failed: %s\n", cmd, requestedPID, strerror(errno));
        } else if (debug_mode == 1) {
            fprintf(stdout, "Sent %s to %d \n", cmd, targetPID);
        }

        if (ret == 0) {
            if (strcmp(cmd, "nuke") == 0) {
                process* curr = global_process_list;
                while (curr) {
                    pid_t pgid = getpgid(curr->pid);
                    if (pgid == requestedPID) {
                        curr->status = TERMINATED;
                    }
                    curr = curr->next;
                }
            } else {
                updateProcessStatus(global_process_list, requestedPID, updatedStatus);
            }
        }

        return ret;
    }

    return 1;
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

        //clears out processes that had been terminated 
        int zombie_process;
        while (waitpid(-1, &zombie_process, WNOHANG) > 0) {
        }

        //auto refresh and print the status of background processes 
        // before presenting the command prompt to the user again
        if (global_process_list != NULL) {
            updateProcessList(&global_process_list);
        }

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
        last_command_tracked = 0;

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

        if (strcmp(parsedLine->arguments[0], "procs") == 0) {
            if (global_process_list != NULL) {
                printProcessList(&global_process_list);
            } else {
                printf("No background processes currently tracking.\n");
            }
            freeCmdLines(parsedLine);
            continue;
        }

        int signal_result = handleSignal(parsedLine);
        if(signal_result == 1){
            excecute(parsedLine);
        }
        if (signal_result != 1 || !last_command_tracked) {
            freeCmdLines(parsedLine);
        }
    }

    freeProcessList(global_process_list);

    return 0;
}