#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    int pipefd[2];
    pid_t child1, child2;

    // creating a pipe
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        exit(EXIT_FAILURE);
    }

    // forking first child process
    fprintf(stderr, "(parent_process>forking…)\n");
    child1 = fork();

    if (child1 == -1) {
        perror("fork 1 failed");
        exit(EXIT_FAILURE);
    }

    if (child1 == 0) {
        fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe…)\n");
        
        close(STDOUT_FILENO);         
        dup(pipefd[1]);              
        close(pipefd[1]);          
        close(pipefd[0]);     

        char *cmd_args[] = {"ps", "-xl", NULL};
        fprintf(stderr, "(child1>going to execute cmd: ps -xl)\n");
        execvp(cmd_args[0], cmd_args);
        
        // if execvp returns an error occurred
        perror("execvp child1 failed");
        _exit(EXIT_FAILURE);
    }

    fprintf(stderr, "(parent_process>created process with id: %d)\n", child1);

    // closing the write end of the pipe in parent
    fprintf(stderr, "(parent_process>closing the write end of the pipe…)\n");
    close(pipefd[1]); 

    // now fork second child process
    fprintf(stderr, "(parent_process>forking…)\n");
    child2 = fork();

    if (child2 == -1) {
        perror("fork 2 failed");
        exit(EXIT_FAILURE);
    }

    if (child2 == 0) {
        fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe…)\n");
        
        close(STDIN_FILENO);          
        dup(pipefd[0]);              
        close(pipefd[0]);

        char *cmd_args[] = {"grep", "5", NULL};
        fprintf(stderr, "(child2>going to execute cmd: grep 5)\n");
        execvp(cmd_args[0], cmd_args);
        
        perror("execvp child2 failed");
        _exit(EXIT_FAILURE);
    }

    fprintf(stderr, "(parent_process>created process with id: %d)\n", child2);

    // closing the read end of the pipe in parent
    fprintf(stderr, "(parent_process>closing the read end of the pipe…)\n");
    close(pipefd[0]);

    // now wait for children to terminate in order of execution
    fprintf(stderr, "(parent_process>waiting for child processes to terminate…)\n");
    waitpid(child1, NULL, 0);
    waitpid(child2, NULL, 0);

    fprintf(stderr, "(parent_process>exiting…)\n");
    return 0;
}