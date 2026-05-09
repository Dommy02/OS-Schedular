#include "utils.h"
#include "math.h"
#include "Mem.h"
#include "headers.h"

//
void clearResources(int);
void schedulerLoop();
void sigchldHandler(int);
PCB *makeProcessPCB(Process *);
void dotLogPrint(PCB *);
void dotPerfPrint();
void timeToEnd(int);
void readAddressInRam(int);
//

////////////////////////////////////////////////////////////////////////////
// Revised Section

Process *receivedProcess;

int messageQueueID;
PCBQueue *processQueue;
PCBQueue *blockedQueue; // blocked queue to be added IMP
fQueue *wTATQueue;
int sharedMemID;
PCB *sharedMemPCBPtr;
PCB *runningProcessPCBPtr;

int isFree = 1;
int isInterrupted = 0;
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
int quantum, N, M, K;
char algorithm[10];
int current_k = 0;

////////////////////////////////////////////////////////////////////////////////////
// phase 2
Ram *ramArray;
int saveMeSemaphore;
int main(int argc, char *argv[])
{

    // printf("#Hi i am the scheduler\n");
    ////////////////////////////////////////////////////////////////////////////////////
    // setting clock and signals
    initClk();
    signal(SIGINT, clearResources);
    signal(SIGCHLD, sigchldHandler);
    signal(SIGUSR1, timeToEnd);
    signal(SIGUSR2, readAddressInRam);

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
        argv[5] = k -> how many quantum's to reset all R bits
    */
    if (argc < 7)
    {
        printf("#Scheduler argv missing values\n");
        exit(-1);
    }
    strcpy(algorithm, argv[1]);
    N = atoi(argv[2]);
    M = atoi(argv[3]);
    quantum = atoi(argv[4]);
    K = atoi(argv[5]);
    saveMeSemaphore = atoi(argv[6]);
    /*// this is 1 CPU FCFS
    if (!strcmp(algorithm, "FCFS"))
    {
        quantum = __INT_MAX__;
    }*/
    // printf("#scheduler post argv\n");
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
    //
    schedulerLoop();

    // finalPrint();

    up(saveMeSemaphore);
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
    if (processQueue)
    {
        for (int i = 0, n = processQueue->size; i < n; i++)
        {
            PCB *delPtr = dequeuePCB(processQueue);
            if (delPtr != NULL)
                free(delPtr);
        }
    }
    if (blockedQueue)
    {
        for (int i = 0, n = blockedQueue->size; i < n; i++)
        {
            PCB *delPtr = dequeuePCB(blockedQueue);
            if (delPtr != NULL)
                free(delPtr);
        }
    }
    if (ramArray)
    {
        if (ramArray->ramArray)
            free(ramArray->ramArray);
        free(ramArray);
    }
    // free(processQueue);

    //

    exit(0);
}

void readAddressInRam(int signum)
{
    // a process reads from the file and raises SIGUSER2 if the penalty == 1 then it continues else it must go to the blocked queue
    // with blocked time = current time + penalty
    if (signum == SIGUSR2)
    {
        int RM = 0;
        if (sharedMemPCBPtr->last_request_state == 'r' || sharedMemPCBPtr->last_request_state == 'R')
            RM = 2;
        else if (sharedMemPCBPtr->last_request_state == 'w' || sharedMemPCBPtr->last_request_state == 'W')
            RM = 3;
        if (RM == 0)
        {
            printf("the RM is wrong\n");
            raise(SIGINT);
            exit(-1);
        }
        int penalty = modifyData(ramArray, sharedMemPCBPtr->PT_index, sharedMemPCBPtr->last_request_hex, RM,
                                 sharedMemPCBPtr->id, sharedMemPCBPtr->limit);

        if (penalty == 0)
        {
            // leave the process
            kill(runningProcessPCBPtr->pid, SIGCONT);
        }
        else
        {
            memcpy(runningProcessPCBPtr, sharedMemPCBPtr, sizeof(PCB));
            isInterrupted = 1;
            runningProcessPCBPtr->p_isInterrupted = 1;
            runningProcessPCBPtr->blocked_time = getClk() + penalty;

            runningProcessPCBPtr->p_state = p_blocked;
            enqueuePCB(blockedQueue, runningProcessPCBPtr);
            //
            runningProcessPCBPtr = NULL;
            nextCPUStartTime = getClk() + 2;
            current_k++;
            up(semSchedulerTurn);
        }
    }
}
void schedulerLoop()
{
    // printf("|##########################|-> Entered schedulerLoop\n");

    runningProcessPCBPtr = NULL;
    isFree = 1;
    isInterrupted = 0;
    finishedProcesses = 0;
    nextCPUStartTime = -1;
    numberOfReceivedProcesses = 0;
    processQueue = createPCBqueue();
    blockedQueue = createPCBqueue();
    wTATQueue = createFQueue();
    int current_k = 0;
    ramArray = start_Ram();

    //
    int lastProcessStartTime = -1;
    // for printing once per second
    int lastTickTime = -1;

    // while (processQueue->size > 0 || blockedQueue->size > 0 || runningProcessPCBPtr != NULL)
    while (isProcessGeneratorDone == 0 || processQueue->size > 0 || blockedQueue->size > 0 || runningProcessPCBPtr != NULL)
    {

        int currentTime = getClk();
        ////////////////////////////////////////////////////////////////////////////////////////////////
        //  time print
        if (lastTickTime < currentTime)
        {
            printf("#####################################################################################################\n");
            printf("Current Time : %d\n", currentTime);
            printf("isProcessGeneratorDone: %d\n", isProcessGeneratorDone);
            printf("processQueue->size: %d\n", processQueue->size);
            printf("blockedQueue->size: %d\n", blockedQueue->size);
            printf("runningProcessPCBPtr != NULL: %s\n", runningProcessPCBPtr != NULL ? "TRUE" : "FALSE");
            printf("#####################################################################################################\n");
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////
        //  R reset
        if (lastTickTime < getClk() && current_k >= K)
        {
            // printf("R reset Block \n");
            current_k = 0;
            clear_R(ramArray->ramArray);
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////
        //  Blocked Queue check
        if ((lastTickTime < getClk()) && blockedQueue->size > 0 && getClk() >= getHeadBlockedTime(blockedQueue))
        {
            // printf("Blocked Queue check Block \n");
            PCB *unblockedProcessPCB = dequeuePCB(blockedQueue);
            unblockedProcessPCB->blocked_time = -1;

            printLoading(ramArray->ramArray, getClk(), unblockedProcessPCB->PT_index, unblockedProcessPCB->last_request_hex,
                         unblockedProcessPCB->base, unblockedProcessPCB->id);

            unblockedProcessPCB->p_state = p_stopped;
            enqueuePCB(processQueue, unblockedProcessPCB);
        }

        ////////////////////////////////////////////////////////////////////////////////////////////////
        //  pre-emptive block
        if ((lastTickTime < getClk()) && runningProcessPCBPtr != NULL && !(isFree || isInterrupted))
        {
            // printf("pre-emptive block Block \n");
            up(semProcessTurn);
            down(semSchedulerTurn);
        }

        if (!(isFree || isInterrupted) && getClk() - lastProcessStartTime >= quantum)
        {
            // printf("quantum is over Block \n");
            current_k++;
            // should say the old process is stopped and the next if should print the new started process
            if (processQueue->size > 0)
            {
                kill(runningProcessPCBPtr->pid, SIGSTOP);
                memcpy(runningProcessPCBPtr, sharedMemPCBPtr, sizeof(PCB));
                dotLogPrint(runningProcessPCBPtr);

                runningProcessPCBPtr->p_state = p_stopped;

                enqueuePCB(processQueue, runningProcessPCBPtr);

                runningProcessPCBPtr = NULL;
                isInterrupted = 1;
                nextCPUStartTime = getClk() + 1;
            }
            else
            {
                lastProcessStartTime = getClk();
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

                // printf("|##########################|-> Process ID : %d, Current Time : %d\n", receivedProcess->id, getClk());

                PCB *newProcessPCB = makeProcessPCB(receivedProcess);
                enqueuePCB(processQueue, newProcessPCB);
                // printf("# hi am a process with data id: %d\n", newProcessPCB->id);
                // printf("# the process queue size -> %d\n", processQueue->size);
                numberOfReceivedProcesses++;
            }
        }

        //

        /*
        if (lastTickTime < getClk())
        {
            printf("isFree - %d \n", isFree);
            printf("isInterrupted - %d \n", isInterrupted);
            printf("getClk() - %d \n", getClk());
            printf("nextCPUStartTime - %d \n", nextCPUStartTime);
            printf("processQueue->size - %d \n", processQueue->size);
        }
        */
        ////////////////////////////////////////////////////////////////////////////////////////////////
        //  the cpu is free or Interrupted
        if ((isFree || isInterrupted) && getClk() >= nextCPUStartTime && processQueue->size > 0)
        {

            // printf("cpu is free or Interrupted Block \n"); // yes
            isFree = 0;
            isInterrupted = 0;

            runningProcessPCBPtr = dequeuePCB(processQueue);
            // printf("Entering ID : %d, Running Process is NULL ? %s\n", runningProcessPCBPtr->id, runningProcessPCBPtr == NULL ? "YES" : "NO");
            // printf("Dequeued process with ID: %d \n", runningProcessPCBPtr->id); // yes

            if (runningProcessPCBPtr->p_state == p_ready && runningProcessPCBPtr->pid == -1)
            {
                // first entry
                // printf("Yippi my first entry\n");
                runningProcessPCBPtr->start_time = getClk();
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
                    perror("this execl failed");
                    exit(-1);
                }
                else if (pid > 0)
                {
                    // parent
                    // printf("I AM HERE Parent!\n");

                    runningProcessPCBPtr->pid = pid;
                    kill(runningProcessPCBPtr->pid, SIGSTOP);
                    runningProcessPCBPtr->PT_index = putForFirstTime(ramArray, runningProcessPCBPtr->limit,
                                                                     runningProcessPCBPtr->id, runningProcessPCBPtr->base, getClk());
                    // printf("I AM HERE Escaped Parent!\n");
                }
            }
            else if (runningProcessPCBPtr->pid > 0)
            {
                // not the first entry
                // kill(runningProcessPCBPtr->pid, SIGSTOP);
                // kill(runningProcessPCBPtr->pid, SIGCONT);
            }

            // printf("phew i survived that shit\n");
            lastProcessStartTime = getClk();

            // Print started or resumed
            dotLogPrint(runningProcessPCBPtr);

            // (SHOULD UPDATE) (NOT DONE) : i should check if current time is actually == getClk() in this second and should update relative time naming
            // getClk() = getClk();
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
    /*
    printf("\n\n#for some mysteries reasons i am out of the main loop and here are my values\n");
    printf("#isProcessGeneratorDone = %d\n", isProcessGeneratorDone);
    printf("#processQueue->size = %d\n", processQueue->size);
    printf("#blockedQueue->size > 0 = %d\n", blockedQueue->size > 0);
    printf("#runningProcessPCBPtr != NULL = %d\n", runningProcessPCBPtr != NULL);
    */
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
        current_k++;
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

        int temp_p_isInterrupted = runningProcessPCBPtr->p_isInterrupted;
        freeProcess(ramArray, runningProcessPCBPtr->PT_index, runningProcessPCBPtr->limit);
        free(runningProcessPCBPtr);
        runningProcessPCBPtr = NULL;
        if (!temp_p_isInterrupted)
        {
            up(semSchedulerTurn);
        }

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
    // phase 2
    newProcessPCB->base = processPtr->base;
    newProcessPCB->limit = processPtr->limit;
    newProcessPCB->PT_index = -1;
    newProcessPCB->blocked_time = -1;
    newProcessPCB->last_request_hex = -1;
    newProcessPCB->last_request_state = 'n';
    newProcessPCB->p_isInterrupted = 0;
    // newProcessPCB->pageTable.pageTableArray = (PT_entry*) malloc(sizeof(PT_entry) * newProcessPCB->limit); // page table is array of like boxes { [V_address : Physical_address and valid bit] [] []  }
    // startPageTab(newProcessPCB->pageTable.pageTableArray, newProcessPCB->limit);
    // end of phase 2

    // (TODO) - the TA said we should fork on arrival and not on starting
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
    // printf("#\tI got that mr process generator\n");
}