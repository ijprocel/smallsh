#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include "built_in.h"
#include "structs.h"
#include "signal_handlers.h"

void parseCommand(Command* command, char* command_str){
    //Parse each space-separated piece of the command string
    command->arguments[0] = strtok(command_str, " ");

    char* arg = strtok(NULL, " ");
    int source_idx = 1;
    //Keep populating the 'arguments' array until the end of the command or a redirection operator is reached.
    while (arg != NULL && strcmp(arg, "<") && strcmp(arg, ">")){
        command->arguments[source_idx++] = arg;
        arg = strtok(NULL, " ");
    }
    //NULL-terminate the argument string
    command->arguments[source_idx] = 0;
    
    //Check if redirection operators are present and update the Command object.
    while (arg != NULL){
        if (!strcmp(arg, "<")){
            arg = strtok(NULL, " ");
            strcpy(command->input, arg);
        }
        else if (!strcmp(arg, ">")){
            arg = strtok(NULL, " ");
            strcpy(command->output, arg);
        }

        arg = strtok(NULL, " ");
    }
}

void getCommand(Command* command) {
    //Set all properties to null
    command->arguments[0] = 0;
    command->input[0] = 0;
    command->output[0] = 0;
    command->foreground = -1;

    printf(":  ");
    fflush(stdout);

    //Buffer is made one char longer than the maximum command length to account for the trailing newline
    char command_str[2049];
    fgets(command_str, 2050, stdin);

    //Erase the newline from the input
    int size = strlen(command_str);
    command_str[--size] = 0;

    /*If the foreground symbol is present, delete it and set or clear the foreground flag, depending on
    whether or not background processes are allowed*/
    if (command_str[size-1] == '&'){
        command_str[--size] = 0;
        command->foreground = fg_only;
    }
    //Otherwise, set the foreground flag
    else {
        command->foreground = 1;
    }

    //Check if the command contains any "$$" substrings that must be expanded into the parent process ID
    char* pid_location = strstr(command_str, "$$");

    if (!pid_location){
        parseCommand(command, command_str);
        return;
    }

    //Code below only executes if a $$ to expand is found
    //Note: expanded command must still be 2048 chars or less

    //Buffer to hold revised command string
    char expanded_command_str[2049];
    memset(expanded_command_str, 0, 2049);
    
    int dest_idx = 0;
    char* pid = command->pid_str;
    int pid_len = strlen(pid);

    //Iterate through original command string
    for (int source_idx=0; source_idx < size; source_idx++){
        int pid_var_found = strncmp(command_str + source_idx, "$$", 2);

        //If the expansion substring is found, copy in the pid.
        if (!pid_var_found){
            strcpy(expanded_command_str + dest_idx, pid);

            //Shift the destination idx so that the next character will be placed immediately after the pid 
            dest_idx = dest_idx + pid_len;

            //Skip the second '$' of the expansion variable
            source_idx++;
        }
        else {
            expanded_command_str[dest_idx] = command_str[source_idx];
            dest_idx++;
        }
    }
    
    parseCommand(command, expanded_command_str);
}

void addChildToList(Node* children, int pid, int foreground){
    Node* newChild = (Node*)malloc(sizeof(Node));
    newChild->pid = pid;
    newChild->next = children->next;
    newChild->foreground = foreground;

    children->next = newChild;
}

void checkForTerminations(Node* children){
    Node* child = children->next;  //Head node contains garbage PID

    //Iterate through children, checking for recently-terminated bg processes
    int childStatus;
    while (child != NULL){
        pid_t terminated = waitpid(child->pid, &childStatus, WNOHANG);
        
        //If a child's foreground flag is cleared, and has just been waited on for the first time, report it.
        if (!child->foreground && (terminated > 0)){
            termProcess termProcess = {terminated, childStatus};
            printf("background pid %d is done: ", terminated);
            fflush(stdout);
            printStatus(&termProcess);

            child->foreground = 1;  //Set the foreground flag so that it will be ignored the next time.
        }

        child = child->next;
    }
}

void setSignalBehavior(int fg){
    //Foreground processes respond to SIGINT in the normal way, bg processes ignore it
    if (fg) {
        set_SIGINT_behavior(SIG_DFL);
    }
    else {
        set_SIGINT_behavior(SIG_IGN);
    }

    //All children ignore SIGTSTP
    set_SIGTSTP_behavior(SIG_IGN);
}

void runCommand(Command command){
    setSignalBehavior(command.foreground);

    int input = 0, output = 0;
    
    //If the user has redirected output, open the appropriate stream.
    //Otherwise, if the command will run in the background, redirect to /dev/null
    if (command.output[0]){
        output = open(command.output, O_WRONLY | O_CREAT, 0700);
        dup2(output, 1);
    }
    else if (!command.foreground){
        output = open("/dev/null", O_WRONLY);
        dup2(output, 1);
    }

    //If the user has redirected input, open the appropriate stream.
    //Otherwise, if the command will run in the background, redirect to /dev/null
    if (command.input[0]){
        input = open(command.input, O_RDONLY);
        dup2(input, 0);
    }
    else if (!command.foreground){
        output = open("/dev/null", O_RDONLY);
        dup2(input, 0);
    }

    //Check for input or output errors
    if (output == -1 || input == -1) {
        perror("open()");
        exit(1);
    }

    //Execute command
    execvp(command.arguments[0], command.arguments);
    perror("execv");   
    exit(EXIT_FAILURE);
}

void customProcess(pid_t* spawnpid, Command command, Node* children, termProcess* last){
    *spawnpid = fork();
    switch(*spawnpid){
        case -1:
            perror("fork() failed!");
            exit(1);
            break;

        case 0:
            runCommand(command);            
            break;
            
        default:
            addChildToList(children, *spawnpid, command.foreground);
            
            if (command.foreground){
                //Set the global variable that informs the signal handlers if there is a foreground process running.
                fg_process_running = 1;

                int childStatus;
                pid_t childPid = waitpid(*spawnpid, &childStatus, 0);

                //Clear the process running flag and save the termination information
                fg_process_running = 0;
                last->pid = childPid;
                last->status = childStatus;
                
                if (WIFSIGNALED(last->status)){
                    printf("terminated by signal %d\n", WTERMSIG(last->status));
                }
            }
            else {
                printf("background pid is %d\n", *spawnpid);
            }

            break;
    }
}

char* pidToString(pid_t pid){
    int str_length = ceil(log10(pid)) + 1;
    char* pid_str = (char*)malloc(str_length * sizeof(char));
    sprintf(pid_str, "%d", pid);
    return pid_str;
}

int main() {
    //Get parent's pid for possible later use in $$ expansion
    char* pid_str = pidToString(getpid());
    printf("\n\nProcess id: %s\n", pid_str);
    fflush(stdout);

    int exit_shell = 0;
    pid_t spawnpid = -5;

    //Initialize a linked list to hold the spawned child proces ids and information.
    Node* children = (Node*)malloc(sizeof(Node));
    children->pid = spawnpid;
    children->next = NULL;
    children->foreground = -1;

    //This struct will contain information about the most recently terminated foreground process
    termProcess lastTermProcess = {-1, 0};
    
    //Set the SIGINT and SIGTSTP handlers for the parent process.
    set_SIGINT_behavior(handle_SIGINT);
    set_SIGTSTP_behavior(handle_SIGTSTP);

    //Initialize an object to hold information about each command entered
    //Array sizes are based on maximum output given in specs
    char* args[512];
    char input[2048];
    char output[2048];
    Command command = {args, -1, input, output, pid_str};

    //Repeatedly prompt the user for commands until they choose to exit
    while (!exit_shell && spawnpid){

        checkForTerminations(children);             //Check if any background processes have terminated

        getCommand(&command);                       //Get the full command input from the user
        char* operation = command.arguments[0];     //Command itself stored as first argument
        
        if (!operation){                            //Handle a null or whitespace input
            command.foreground = -1;
            continue;
        }
        else if (!strcmp(operation, "exit")){
            killChildren(children);
            exit_shell = 1;
        }
        else if (!strcmp(operation, "cd")){
            char* fpath = command.arguments[1];
            cd(fpath);
        }
        else if (!strcmp(operation, "status")){
            printStatus(&lastTermProcess);
        }
        else if (operation[0] != '#'){
            customProcess(&spawnpid, command, children, &lastTermProcess);
        }
        
        command.foreground = -1;
    }

    //Free dynamically allocated memory
    _freeNodes(children);
    free(pid_str);

    if (spawnpid){
        printf("Goodbye!\n");
        fflush(stdout);
    }
    
    return 0;
}