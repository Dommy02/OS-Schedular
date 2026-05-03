#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>
#include <stdio.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <errno.h>

typedef enum Process_State
{
    p_ready,
    p_stopped,
    p_finished,
    p_running
} Process_State;
//

typedef struct PCB
{
    pid_t pid;
    int id;
    int arrival_time;
    int runtime;
    int start_time;
    int relative_time;
    int finish_time;
    int remaining_time;
    int priority;
    Process_State p_state;
    int last_start_time;
    // phase 2 updates
    // PT_entry* PT;
    
} PCB;

typedef struct NodePCB
{
    PCB *ptrProcessPCB;
    struct NodePCB *next;
} NodePCB;

typedef struct
{
    NodePCB *head;
    NodePCB *tail;
    int size;
} PCBQueue;

NodePCB *create_nodePCB(PCB *ptrProcessPCB);
/////////////////////////////////////////////////////////////////////
typedef struct Nodef
{
    float value;
    struct Nodef *next;
} Nodef;

typedef struct
{
    Nodef *head;
    Nodef *tail;
    int size;
} fQueue;

Nodef *create_nodef(float value);

//////////////////////////////////

PCBQueue *createPCBqueue();
void enqueuePCB(PCBQueue *q, PCB *ptrProcessPCB);
PCB *dequeuePCB(PCBQueue *q);

//////////////////////////////////
fQueue *createFQueue();
void enqueueF(fQueue *q, float value);
float dequeueF(fQueue *q);
//////////////////////////////////

void down(int sem);
void up(int sem);

#endif