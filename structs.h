#ifndef MY_STRUCTS
#define MY_STRUCTS

#include <stdio.h>
#include <stdlib.h>

typedef struct Node Node;

//This structure will be used to form a linked list of child processes.
struct Node {
    int pid;
    Node* next;
    int foreground;
};

void _freeNodes(Node* head){
    while (head != NULL){
        Node* temp = head->next;
        free(head);
        head = temp;
    }
}

//Holds information about the most recently terminated process.
typedef struct termProcess {
    pid_t pid;
    int status;
} termProcess;

//Contains the results of command parsing
typedef struct Command {
    char** arguments;
    int foreground;
    char* input;
    char* output;
    char* pid_str;
} Command;

#endif