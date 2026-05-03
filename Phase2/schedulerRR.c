#include "utils.h"
#include "math.h"
#include "Mem.h"

typedef struct
{
    long m_type;
    int id;
    int arrival_time;
    int runtime;
    int priority;
} Process;

//
void clearResources(int);
void schedulerLoop();
void sigchldHandler(int);
PCB *makeProcessPCB(Process *);
void dotLogPrint(PCB *);
void dotPerfPrint();
void timeToEnd(int);
//

////////////////////////////////////////////////////////////////////////////
// Revised Section

Process *receivedProcess;

int messageQueueID;
PCBQueue *processQueue;
fQueue *wTATQueue;
int sharedMemID;
PCB *sharedMemPCBPtr;
PCB *runningProcessPCBPtr;

int isFree = 1;
int isInterupted = 0;
int finishedProcesses = 0;
int nextCPUStartTime = -1;

int CPUEndTime = -1;
int CPURunTime = 0;
int CPUProcessesTotalWaitTime = 0;
float CPUProcessesTotalWTA = 0;

int semProcessTurn = -1;
int semSchedulerTurn = -1;

FILE *dotLogFilePtr;
FILE *dotPerfFilePtr;

int isProcessGeneratorDone = 0;

int numberOfReceivedProcesses = 0;

////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////
// argc and argv data
int quantum, N, M;
char algorithm[10];

int main(int argc, char *argv[])
{

    ////////////////////////////////////////////////////////////////////////////////////
    // setting clock and signals
    initClk();
    signal(SIGINT, clearResources);
    signal(SIGCHLD, sigchldHandler);
    signal(SIGUSR1, timeToEnd);

    ////////////////////////////////////////////////////////////////////////////////////
    // file opening
    dotLogFilePtr = fopen("Scheduler.log", "w");
    if (dotLogFilePtr == NULL)
    {
        perror("Failed to open Scheduler.log\n");
        exit(-1);
    }

    dotPerfFilePtr = fopen("Scheduler.pref", "w");
    if (dotLogFilePtr == NULL)
    {
        perror("Failed to open Scheduler.pref\n");
        exit(-1);
    }

    ////////////////////////////////////////////////////////////////////////////////////
    /*
        argc = 6
        argv[0] = scheduler.out
        argv[1] = algorithm
        argv[2] = N
        argv[3] = M
        argv[4] = quantum
    */
    if (argc < 5)
    {
        printf("Scheduler argv missing values\n");
        exit(-1);
    }
    strcpy(algorithm, argv[1]);
    N = atoi(argv[2]);
    M = atoi(argv[3]);
    quantum = atoi(argv[4]);
    /*// this is 1 CPU FCFS
    if (!strcmp(algorithm, "FCFS"))
    {
        quantum = __INT_MAX__;
    }*/

    ////////////////////////////////////////////////////////////////////////////////////
    // message queue setup
    key_t key = ftok("keyfile", 'a');
    if (key == -1)
    {
        perror("ftok failed");
        exit(-1);
    }
    messageQueueID = msgget(key, 0666 | IPC_CREAT);
    if (messageQueueID == -1)
    {
        perror("Error in Message Queue Creation");
        exit(-1);
    }

    ////////////////////////////////////////////////////////////////////////////////////
    // received processes setup

    receivedProcess = (Process *)malloc(sizeof(Process));
    if (receivedProcess == NULL)
    {
        perror("malloc failed");
        exit(-1);
    }

    ////////////////////////////////////////////////////////////////////////////////////
    // shared memory setup
    key_t key2 = ftok("keyfile", 'm');
    if (key2 == -1)
    {
        perror("ftok failed");
        exit(-1);
    }
    sharedMemID = shmget(key2, sizeof(PCB), IPC_CREAT | 0666);
    if (sharedMemID == -1)
    {
        perror("Error in Shared Memory Creation in Scheduler");
        exit(-1);
    }
    sharedMemPCBPtr = (PCB *)shmat(sharedMemID, NULL, 0);

    ////////////////////////////////////////////////////////////////////////////////////
    // semaphored setup

    semProcessTurn = semget(IPC_PRIVATE, 1, 0666 | IPC_CREAT);
    semSchedulerTurn = semget(IPC_PRIVATE, 1, 0666 | IPC_CREAT);

    if (semSchedulerTurn == -1 || semProcessTurn == -1)
    {
        perror("semget failed");
        exit(-1);
    }

    union Semun semun;
    semun.val = 0;

    if (semctl(semProcessTurn, 0, SETVAL, semun) == -1 || semctl(semSchedulerTurn, 0, SETVAL, semun) == -1)
    {
        perror("semctl Failed in Scheduler");
        exit(-1);
    }

    ////////////////////////////////////////////////////////////////////////////////////
    // TODO implement the scheduler :)
    schedulerLoop();

    // finalPrint();

    ////////////////////////////////////////////////////////////////////////////////////
    // upon termination release the clock resources.
    destroyClk(true);
}

void clearResources(int signum)
{
    
    printf("Scheduler Cleared It's Stuff\n");

    if (sharedMemID != -1)
        shmctl(sharedMemID, IPC_RMID, NULL);

    if (dotLogFilePtr != NULL)
        fclose(dotLogFilePtr);
    if (dotPerfFilePtr != NULL)
        fclose(dotPerfFilePtr);
    

    if (semProcessTurn != -1)
        semctl(semProcessTurn, 0, IPC_RMID);
    if (semSchedulerTurn != -1)
        semctl(semSchedulerTurn, 0, IPC_RMID);

    
    
    /*
    if (receivedProcess != NULL)
    {
        free(receivedProcess);
        receivedProcess = NULL;
    }
    */
    
    

    // (SHOULD UPDATE) (DONE) : Dont Use This instead loop on the size of the queue use dequeue and free the returned PCB pointer
    for (int i = 0, n = processQueue->size; i < n; i++)
    {
        PCB *delPtr = dequeuePCB(processQueue);
        if (delPtr != NULL)
            free(delPtr);
    }
    //free(processQueue);

    //

    exit(0);
}

void schedulerLoop()
{
    // printf("|##########################|-> Entered schedulerLoop\n");

    runningProcessPCBPtr = NULL;
    isFree = 1;
    isInterupted = 0;
    finishedProcesses = 0;
    nextCPUStartTime = -1;
    numberOfReceivedProcesses = 0;
    processQueue = createPCBqueue();
    wTATQueue = createFQueue();

    //
    int lastProcessStartTime = -1;
    // for printing once per second
    int lastTickTime = -1;
    // while (finishedProcesses < number_of_processes)
    while (isProcessGeneratorDone == 0 || processQueue->size > 0 || runningProcessPCBPtr != NULL)
    {

        int currentTime = getClk();
        ////////////////////////////////////////////////////////////////////////////////////////////////
        //  time print
        if (lastTickTime < currentTime)
        {
            printf("Current Time : %d #\n", currentTime);
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////
        //  pre-emptive block
        if ((lastTickTime < currentTime) && runningProcessPCBPtr != NULL && !(isFree || isInterupted))
        {
            //  this had to be every loop since the critical section isn't every loop

            // printf("In parent the semProcessTurn Before : %d\n", semProcessTurn);
            up(semProcessTurn);
            // printf("In parent the semProcessTurn After : %d\n", semProcessTurn);

            // printf("In parent the semSchedulerTurn Before : %d\n", semSchedulerTurn);
            down(semSchedulerTurn);
            // printf("In parent the semSchedulerTurn After : %d\n", semSchedulerTurn);
        }
        if (!(isFree || isInterupted) && currentTime - lastProcessStartTime >= quantum)
        {
            // should say the old process is stopped and the next if should print the new started process
            if (processQueue->size > 0)
            {
                kill(runningProcessPCBPtr->pid, SIGSTOP);
                memcpy(runningProcessPCBPtr, sharedMemPCBPtr, sizeof(PCB));
                dotLogPrint(runningProcessPCBPtr);

                runningProcessPCBPtr->p_state = p_stopped;

                enqueuePCB(processQueue, runningProcessPCBPtr);

                runningProcessPCBPtr = NULL;
                isInterupted = 1;
                nextCPUStartTime = currentTime + 1;
            }
            else
            {
                lastProcessStartTime = currentTime;
            }
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////
        //  process generator message queue
        int receiveValue = 0;
        while (receiveValue != -1)
        {
            receiveValue = msgrcv(messageQueueID, receivedProcess, sizeof(Process), 1, IPC_NOWAIT);
            if (receiveValue != -1)
            {
                // printf("|##########################|-> Process ID : %d, Current Time : %d\n", receivedProcess->id, currentTime);

                PCB *newProcessPCB = makeProcessPCB(receivedProcess);

                enqueuePCB(processQueue, newProcessPCB);
                numberOfReceivedProcesses++;
            }
        }

        //

        ////////////////////////////////////////////////////////////////////////////////////////////////
        //  the cpu is free or interupted
        if ((isFree || isInterupted) && currentTime >= nextCPUStartTime && processQueue->size > 0)
        {
            isFree = 0;
            isInterupted = 0;

            runningProcessPCBPtr = dequeuePCB(processQueue);
            // printf("Entering ID : %d, Running Process is NULL ? %s\n", runningProcessPCBPtr->id, runningProcessPCBPtr == NULL ? "YES" : "NO");

            if (runningProcessPCBPtr->p_state == p_ready && runningProcessPCBPtr->pid == -1)
            {
                // first entry
                runningProcessPCBPtr->start_time = currentTime;
                pid_t pid = fork();

                if (pid == 0)
                {
                    // child
                    char sharedMemIDStr[12];
                    char semProcessTurnStr[12], semSchedulerTurnStr[12];

                    snprintf(sharedMemIDStr, sizeof(sharedMemIDStr), "%d", sharedMemID);
                    sprintf(semProcessTurnStr, "%d", semProcessTurn);
                    sprintf(semSchedulerTurnStr, "%d", semSchedulerTurn);

                    execl("./processRR.out", "processRR.out", sharedMemIDStr, semProcessTurnStr, semSchedulerTurnStr, NULL);
                    perror("execl failed");
                    exit(-1);
                }
                else if (pid > 0)
                {
                    // parent
                    runningProcessPCBPtr->pid = pid;
                    kill(runningProcessPCBPtr->pid, SIGSTOP);
                }
            }
            else if (runningProcessPCBPtr->pid > 0)
            {
                // not the first entry
                // kill(runningProcessPCBPtr->pid, SIGSTOP);
                // kill(runningProcessPCBPtr->pid, SIGCONT);
            }

            lastProcessStartTime = currentTime;

            // Print started or resumed
            dotLogPrint(runningProcessPCBPtr);

            // (SHOULD UPDATE) (NOT DONE) : i should check if current time is actually == getClk() in this second and should update relative time naming
            // currentTime = getClk();
            runningProcessPCBPtr->relative_time = getClk(); // relative start time
            runningProcessPCBPtr->p_state = p_running;

            memcpy(sharedMemPCBPtr, runningProcessPCBPtr, sizeof(PCB));
            kill(runningProcessPCBPtr->pid, SIGCONT);
            // up to a semaphore
        }

        lastTickTime = currentTime;
    }

    CPUEndTime = getClk();
    dotPerfPrint();
    // printf("|##########################|-> Exited schedulerLoop\n");
}

void sigchldHandler(int signum)
{
    // i must call sigstop at the start of the child process to make sure i initialize the shared memory

    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG | WUNTRACED);
    if (pid <= 0)
        return;

    if (signum == SIGCHLD && WIFEXITED(status) && !WIFSTOPPED(status))
    {

        if (sharedMemPCBPtr->id != runningProcessPCBPtr->id)
        {
            printf("There is an Error with the PCB Pointer\n");
            raise(SIGINT);
        }
        memcpy(runningProcessPCBPtr, sharedMemPCBPtr, sizeof(PCB));

        runningProcessPCBPtr->p_state = p_finished;
        runningProcessPCBPtr->finish_time = getClk();

        // printf("##########################################\n");
        // printf("Process is Finished\n");
        // printf("ID : %d, Finish Time :%d\n", runningProcessPCBPtr->id, runningProcessPCBPtr->finish_time);

        dotLogPrint(runningProcessPCBPtr);

        int TAT = runningProcessPCBPtr->finish_time - runningProcessPCBPtr->arrival_time;
        float wTAT = (float)TAT / runningProcessPCBPtr->runtime;
        int WAIT_TIME = TAT - runningProcessPCBPtr->runtime;
        //
        CPURunTime += runningProcessPCBPtr->runtime;
        CPUProcessesTotalWaitTime += WAIT_TIME;
        CPUProcessesTotalWTA += wTAT;

        finishedProcesses++;
        nextCPUStartTime = runningProcessPCBPtr->finish_time + 1;
        isFree = 1;

        // printf("Process with ID : %d is Done\n", runningProcessPCBPtr->id);
        enqueueF(wTATQueue, wTAT);
        free(runningProcessPCBPtr);
        runningProcessPCBPtr = NULL;

        up(semSchedulerTurn);

        //(SHOULD UPDATE) (DONE) : dont forget this when adding back semaphores
    }
    /*
    else if (WIFSTOPPED(status))
    {
        // down to a semaphore
    }
    */
}

PCB *makeProcessPCB(Process *processPtr)
{
    PCB *newProcessPCB = (PCB *)malloc(sizeof(PCB));

    newProcessPCB->id = processPtr->id;
    newProcessPCB->arrival_time = processPtr->arrival_time;
    newProcessPCB->runtime = processPtr->runtime;
    newProcessPCB->start_time = -1;
    newProcessPCB->relative_time = -1; // recent start time (Used in RR when a process enters the cpu again)
    newProcessPCB->finish_time = -1;
    newProcessPCB->remaining_time = newProcessPCB->runtime;
    newProcessPCB->priority = processPtr->priority;
    newProcessPCB->p_state = p_ready;

    newProcessPCB->pid = -1;
    /*
    pid_t pid = fork();

    if (pid == 0)
    {
        char sharedMemIDStr[12];
        char semProcessTurnStr[12], semSchedulerTurnStr[12];

        snprintf(sharedMemIDStr, sizeof(sharedMemIDStr), "%d", sharedMemID);
        sprintf(semProcessTurnStr, "%d", semProcessTurn);
        sprintf(semSchedulerTurnStr, "%d", semSchedulerTurn);

        execl("./process.out", "process.out", sharedMemIDStr, semProcessTurnStr, semSchedulerTurnStr, NULL);
        perror("execl failed");
        exit(-1);
    }
    else
    {
        newProcessPCB->pid = pid;
        kill(newProcessPCB->pid, SIGSTOP);
    }
    */

    return newProcessPCB;
}

void dotLogPrint(PCB *runningProcessPCBPtr)
{
    // #At time x process y state arr w total z remain y wait k

    // if ready -> print started
    // if stopped -> print resumed
    // if finished -> print finished
    // if running -> we dont print

    int current_time = getClk();

    // total time i am here - time i was executing
    int Wait_Time = (current_time - runningProcessPCBPtr->arrival_time) - (runningProcessPCBPtr->runtime - runningProcessPCBPtr->remaining_time);
    //

    char str_state[20];
    if (runningProcessPCBPtr->p_state == p_ready)
        strcpy(str_state, "started");
    else if (runningProcessPCBPtr->p_state == p_stopped)
        strcpy(str_state, "resumed");
    else if (runningProcessPCBPtr->p_state == p_running)
        strcpy(str_state, "stopped");
    else if (runningProcessPCBPtr->p_state == p_finished)
        strcpy(str_state, "finished");
    else
    {
        // error
        strcpy(str_state, "U HAVE AN ERROR");
    }

    fprintf(dotLogFilePtr, "At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d",
            current_time, runningProcessPCBPtr->id, str_state, runningProcessPCBPtr->arrival_time, runningProcessPCBPtr->runtime, runningProcessPCBPtr->remaining_time, Wait_Time);

    if (runningProcessPCBPtr->p_state == p_finished)
    {
        int TAT = runningProcessPCBPtr->finish_time - runningProcessPCBPtr->arrival_time;
        float wTAT = (float)TAT / runningProcessPCBPtr->runtime;
        fprintf(dotLogFilePtr, "\tTA\t%d\tWTA\t%.2f", TAT, wTAT);
    }
    fprintf(dotLogFilePtr, "\n");
}

void dotPerfPrint()
{
    float CPUUtilization = ((float)CPURunTime / CPUEndTime) * 100;
    float AvgWTA = CPUProcessesTotalWTA / finishedProcesses;
    float AvgWaiting = (float)CPUProcessesTotalWaitTime / finishedProcesses;

    // float Std
    float variance = 0;
    while (wTATQueue->size > 0)
    {
        float wta = dequeueF(wTATQueue);
        float diff = wta - AvgWTA;
        variance += diff * diff;
    }

    float stdWTA = sqrt(variance / finishedProcesses);

    fprintf(dotPerfFilePtr, "CPU utilization = %.2f%%\n", CPUUtilization);
    fprintf(dotPerfFilePtr, "Avg WTA = %.2f\n", AvgWTA);
    fprintf(dotPerfFilePtr, "Avg Waiting = %.2f\n", AvgWaiting);
    fprintf(dotPerfFilePtr, "Std WTA = %.2f\n", stdWTA);
}

void timeToEnd(int signum)
{
    isProcessGeneratorDone = 1;
}