#include "headers.h"

typedef struct process{
    long mtype;
    int id,arrival,runTime,priority;
    char state;
    int remainigTime,WaitingTime,whichCpu;
} process;

void clearResources(int);

int countLines(const char *fileName)
{
    FILE *fp = fopen(fileName, "r");
    if (!fp)
    {
        fprintf(stderr, "Error: File does not exist.\n");
        exit(1);
    }

    int count = 0;
    char arr[1024];
    while (fgets(arr, 1024, fp) != NULL)
        count++;

    fclose(fp);
    return count;
}

int main(int argc, char * argv[])
{
    signal(SIGINT, clearResources);

    int processesNum = countLines("processes.txt") - 1; // -1 due to the first line
    if (processesNum <= 0)
    {
        fprintf(stderr, "No valid process rows found in processes.txt\n");
        exit(1);
    }

    process* arr = (process*) malloc(processesNum * sizeof(process));
    if (!arr)
    {
        perror("malloc failed");
        exit(1);
    }
    
    FILE *fp = fopen("processes.txt", "r");
    if (!fp)
    {
        perror("fopen failed");
        free(arr);
        exit(1);
    }

    char buffer[100];
    fgets(buffer, sizeof(buffer), fp); // skip header

    int counter = 0;
    while (counter < processesNum && fscanf(fp, "%d %d %d %d", &arr[counter].id, &arr[counter].arrival, &arr[counter].runTime, &arr[counter].priority) == 4)
    {
        arr[counter].mtype = 1;
        arr[counter].state = 'N';
        arr[counter].remainigTime = arr[counter].runTime;
        arr[counter].WaitingTime = 0;
        arr[counter].whichCpu = 0;
        counter++;
    }
    fclose(fp);

    // Ask the user for the chosen scheduling algorithm and its parameters
    int algNum = 0, rrSlot = 0, N = 0, M = 0;
    printf("Choose an algorithm: 1)HPF  2)RR   3)2-cpu FCFS: ");
    scanf("%d", &algNum);
    if (algNum == 2)
    {
        printf("Enter your time slot: ");
        scanf("%d", &rrSlot);
    }
    if(algNum == 3){
        printf("Enter your time slot \"N\" and ThresHold \"M\" : ");
        scanf("%d %d", &N, &M );
    }

    // Start clock
    int clkPid = fork();
    if (clkPid == 0)
    {
        char *clk_argv[] = {"./clk.out", NULL};
        execv("./clk.out", clk_argv);
        perror("execv clk.out failed");
        exit(1);
    }

    int process_index = 0;
    key_t msgQueue_key = ftok("clk.c", 10);
    int msgQueue_id = msgget(msgQueue_key, 0666 | IPC_CREAT);
    if (msgQueue_id == -1)
    {
        perror("msgget failed");
        free(arr);
        destroyClk(true);
        exit(1);
    }

    msgQueue_id = msgget(msgQueue_key, 0666 | IPC_CREAT);
    if (msgQueue_id == -1)
    {
        perror("msgget recreate failed");
        free(arr);
        destroyClk(true);
        exit(1);
    }

    // Data sync for scheduler
    key_t sharedMemKey = ftok("clk.c", 90);
    int sendingData = shmget(sharedMemKey, sizeof(generator_scheduler), 0666 | IPC_CREAT);
    generator_scheduler *schedulerData = (generator_scheduler*) shmat(sendingData, NULL, 0);
    int generator_sem = semget(sharedMemKey, 1, 0666 | IPC_CREAT);
    semctl(generator_sem, 0, SETVAL, 1);

    schedulerData->IamFinished = false;
    schedulerData->IamSendingNow = false;
    schedulerData->NumSendingMess = 0;
    schedulerData->sem_id = generator_sem;
    int schedulerPid = fork();
    if (schedulerPid == 0)
    {
        char processNumStr[16], algStr[16], rrStr[16], Nstr[16],Mstr[16],SharedMem_str[16];
        sprintf(processNumStr, "%d", processesNum);
        sprintf(algStr, "%d", algNum);
        sprintf(rrStr, "%d", rrSlot);
        sprintf(Nstr, "%d", N);
        sprintf(Mstr, "%d", M);
        sprintf(SharedMem_str, "%d", sendingData);

        char *sch_argv[] = {"./scheduler.out", processNumStr, algStr, rrStr,Nstr,Mstr,SharedMem_str,NULL};
        execv("./scheduler.out", sch_argv);
        perror("execv scheduler.out failed");
        exit(1);
    }
    
    initClk();

    while (process_index < processesNum)
    {
        sem_wait(generator_sem);
        schedulerData->IamSendingNow = false;
        sem_signal(generator_sem);
        int now = getClk();
        while (process_index < processesNum && arr[process_index].arrival <= now)
        {
            sem_wait(generator_sem);
            schedulerData->IamSendingNow = true;
            sem_signal(generator_sem);
            if (msgsnd(msgQueue_id, &arr[process_index], sizeof(process) - sizeof(long), !IPC_NOWAIT) != -1)
            {
                process_index++;
            }
        }
    }
    sem_wait(generator_sem);
    schedulerData->IamSendingNow = false;
    schedulerData->IamFinished = true;
    sem_signal(generator_sem);

    free(arr);
    int stat;
    wait(&stat);
    if (!(stat& 0x00FF))
        printf("Scheduler id : %d, finished %d \n",schedulerPid,stat >> 8);
    clearResources(1);
    exit(0);
}

void clearResources(int signum)
{
    signal(SIGINT, SIG_DFL);
    destroyClk(true);
}
