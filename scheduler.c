#include "headers.h"

typedef struct process{
    long mtype;
    int id,arrival,runTime,priority;
    char state;
    int remainigTime,WaitingTime,whichCpu;
} process;

int main(int argc, char * argv[])
{
    int totalProcesses = atoi(argv[1]);

    initClk();
    printf("Scheduler started at t=%d\n", getClk());

    process *PCB = (process *)malloc(sizeof(process) * (1+totalProcesses));// 1 indexed
    process *readyQueue = (process *)malloc(sizeof(process) * (1+totalProcesses));

    int head = 0, tail = 0;
    int receivedCount = 0, finishedCount = 0;

    key_t msgQueue_key = ftok("clk.c", 10);
    int msgQueue_key_id = msgget(msgQueue_key, 0666 | IPC_CREAT);
    
    bool cpuBusy = false;
    pid_t runningPid = -1;
    int runningId = -1;

    while (finishedCount < totalProcesses)
    {
        // Receive all currently available processes from bus
        while (receivedCount < totalProcesses)
        {
            process messageBuff;
            msgrcv(msgQueue_key_id, &messageBuff, sizeof(process) - sizeof(long), 1, IPC_NOWAIT);

            int id = messageBuff.id;
            PCB[id] = messageBuff;

            // Insert in Ready Queue
            if (tail <= totalProcesses)
            {
                readyQueue[tail++] = messageBuff;
                printf("Enqueued P%d at t=%d (runtime=%d)\n", id, getClk(), messageBuff.runTime);
            }
            receivedCount++;
        }

        // Dispatch FCFS if CPU idle
        if (!cpuBusy && head < tail)
        {
            process p = readyQueue[head++];
            runningId = p.id;
            pid_t pid = fork();
            if (pid == -1)
            {
                perror("fork failed");
                free(PCB);
                
                free(readyQueue);
                destroyClk(false);
                return 1;
            }

            if (pid == 0)
            {
                
                for (int rem = p.runTime; rem > 0; rem--)
                {
                    printf("P%d remaining=%d\n", p.id, rem);
                    sleep(1);
                }
                printf("P%d remaining=0\n", p.id);
                _exit(0);
            }

            runningPid = pid;
            cpuBusy = true;
            printf("Started P%d at t=%d\n", runningId, getClk());
        }

        // Check running child
        if (cpuBusy)
        {
            int status;
            pid_t w = waitpid(runningPid, &status, WNOHANG);
            if (w == runningPid)
            {
                
                finishedCount++;
                printf("Finished P%d at t=%d\n", runningId, getClk());
                cpuBusy = false;
                runningPid = -1;
                runningId = -1;
            }
        }
    }

    free(PCB);
    
    free(readyQueue);
    destroyClk(false);
    return 0;
}
