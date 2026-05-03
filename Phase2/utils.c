#include "utils.h"

////////////////////////////////////////////////////////////////
// NodePCB

NodePCB *create_nodePCB(PCB *ptrProcessPCB)
{
    NodePCB *new_node = (NodePCB *)malloc(sizeof(NodePCB));

    new_node->ptrProcessPCB = ptrProcessPCB;
    new_node->next = NULL;

    return new_node;
}

////////////////////////////////////////////////////////////////
// Queue

PCBQueue *createPCBqueue()
{
    PCBQueue *new_queue = (PCBQueue *)malloc(sizeof(PCBQueue));

    new_queue->head = NULL;
    new_queue->tail = NULL;
    new_queue->size = 0;

    return new_queue;
}

void enqueuePCB(PCBQueue *q, PCB *ptrProcessPCB)
{
    NodePCB *new_node = create_nodePCB(ptrProcessPCB);
    if (q->head == NULL)
    {
        q->head = new_node;
    }
    else
    {
        q->tail->next = new_node;
    }

    q->size++;
    q->tail = new_node;
}

PCB *dequeuePCB(PCBQueue *q)
{
    if (q->head == NULL)
    {
        return NULL;
    }

    NodePCB *node = q->head;
    PCB *ptrPCB = node->ptrProcessPCB;

    q->head = q->head->next;

    if (q->head == NULL)
    {
        // the head and the tail were pointing to the same node
        q->tail = NULL;
    }

    q->size--;
    free(node);

    return ptrPCB;
}

////////////////////////////////////////////////////////////////
Nodef *create_nodef(float value)
{
    Nodef *new_node = (Nodef *)malloc(sizeof(Nodef));

    new_node->value = value;
    new_node->next = NULL;

    return new_node;
}
////////////////////////////////////////////////////////////////

fQueue *createFQueue()
{
    fQueue *new_queue = (fQueue *)malloc(sizeof(fQueue));

    new_queue->head = NULL;
    new_queue->tail = NULL;
    new_queue->size = 0;

    return new_queue;
}
void enqueueF(fQueue *q, float value)
{
    Nodef *new_node = create_nodef(value);
    if (q->head == NULL)
    {
        q->head = new_node;
    }
    else
    {
        q->tail->next = new_node;
    }

    q->size++;
    q->tail = new_node;
}
float dequeueF(fQueue *q)
{
    if (q->head == NULL)
    {
        return -1;
    }

    Nodef *node = q->head;
    int value = node->value;

    q->head = q->head->next;

    if (q->head == NULL)
    {
        // the head and the tail were pointing to the same node
        q->tail = NULL;
    }

    q->size--;
    free(node);

    return value;
}
////////////////////////////////////////////////////////////////
// Semaphore

void down(int sem)
{
    struct sembuf op;

    op.sem_num = 0;
    op.sem_op = -1;
    op.sem_flg = !IPC_NOWAIT;

    while (semop(sem, &op, 1) == -1)
    {
        if (errno == EINTR)
            continue;
        perror("Error in semaphored down function");
        exit(-1);
    }
}

void up(int sem)
{
    struct sembuf op;

    op.sem_num = 0;
    op.sem_op = 1;
    op.sem_flg = !IPC_NOWAIT;

    while (semop(sem, &op, 1) == -1)
    {
        if (errno == EINTR)
            continue;
        perror("Error in semaphored up function");
        exit(-1);
    }
}