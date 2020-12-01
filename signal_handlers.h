#ifndef MY_HANDLERS
#define MY_HANDLERS

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

int fg_only = 0;
int fg_process_running = 0;

void handle_SIGINT(int signo){
    /*This is the SIGINT handler for parent process. It handles the
    standard output if a SIGINT is sent when there is no foreground
    process running.

    Standard output in the case that a foreground process is running
    is handled elsewhere to reduce the need for global variables*/

    char* message = "\n:  ";
    int length = fg_process_running ? 0 : 4;        
    write(STDOUT_FILENO, message, length);
    fflush(stdout);
}

void set_SIGINT_behavior(void (*handler)(int)){
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = handler;

    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = SA_RESTART;
    
    sigaction(SIGINT, &SIGINT_action, NULL);
}

void handle_SIGTSTP(int signo){
    /*This is the SIGTSP handler for parent process. It displays the
    message about switching modes. The last four characters of the message
    are only written if the signal arrives when there is no foreground process
    running.
    
    Otherwise, an extra set of brackets and spaces would be printed.*/

    char* message;
    fg_only = !fg_only;

    int length = -1;

    if (fg_only){
        message = "\nEntering foreground-only mode (& is now ignored)\n:  ";
        length = fg_process_running ? 50: 54;
    }
    else {
        message = "\nExiting foreground-only mode\n:  ";
        length = fg_process_running ? 30: 34;
    }

	write(STDOUT_FILENO, message, length);
    fflush(stdout);
}

void set_SIGTSTP_behavior(void (*handler)(int)){
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = handler;

    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);
}

#endif