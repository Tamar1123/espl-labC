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

int main(int argc, char** argv){
    int pipefd[2];

    if (argc != 2) {
        perror("Wrong number of arguments! should have 1 argument.");   
        return 1;
    }

    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        return 1;
    }
    ssize_t msgSize = strlen(argv[1]) + 1; // plus one for the null terminator
    
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return 1;
    }
    if (pid != 0) { //this is the parent process
        if (write(pipefd[1], argv[1], msgSize) == -1) {
            perror("Failed to write.");
            return 1;
        if(close(pipefd[1]) != 0) {
            perror("Failed to close write queue.");
            return 1;
        }
    }
         
    } else { //this is the child process
        char readbuff[msgSize];
        if (read(pipefd[0], readbuff, msgSize) == -1) {
            perror("Failed to read.");
            return 1;
        }
        printf("%s\n", readbuff);

        if(close(pipefd[0]) != 0) {
            perror("Failed to close read queue.");
            return 1;
        }
    }
    return 0;
}


    