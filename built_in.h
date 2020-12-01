#ifndef BUILT_IN_FUNCS
#define BUILT_IN_FUNCS

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "structs.h"

void cd(char* fpath) {
    char* new_path;
    
    if (!fpath){
        char* home = "HOME";
        new_path = getenv(home);
    }
    else {
        new_path = fpath;
    }
    
    chdir(new_path);
    
    long size;
    char *buf;
    char *ptr;
    size = pathconf(".", _PC_PATH_MAX); //Get maximum file name length

    //Get the current working directory and display it to the user.
    if ((buf = (char *)malloc((size_t)size)) != NULL){
        ptr = getcwd(buf, (size_t)size);
    }

    printf("Cwd: %s\n", ptr);
    fflush(stdout);
}

void killChildren(Node* children){
    Node* child = children->next;  //Head node contains garbage PID

    //Iterate through the linked list of child processes and kill them
    int childStatus;
    while (child != NULL){
        kill(child->pid, SIGKILL);
        pid_t killed = waitpid(child->pid, &childStatus, 0);
        child = child->next;
    }
}

void printStatus(termProcess* last) {
    
    if (WIFEXITED(last->status)){
        printf("exit value %d\n", WEXITSTATUS(last->status));
    }
    else if (WIFSIGNALED(last->status)){
        printf("terminated by signal %d\n", WTERMSIG(last->status));
    }
    
    fflush(stdout);
}

#endif