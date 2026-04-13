#include "headers.h"
typedef struct process{
    long mtype;
    int id,arrival,runTime,priority;
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
    // TODO Initialization
    // 1. Read the input files.
    int processesNum = countLines("processes.txt") - 1;// -1 duo to the first line
    process* arr; 
    arr = (process*) malloc(processesNum * sizeof(process));
    FILE *fp = fopen("processes.txt", "r");
    char buffer[100];
    fgets(buffer, sizeof(buffer), fp);
    int counter = 0;
    while(counter != processesNum){
        fscanf(fp,"%d %d %d %d",&arr[counter].id,&arr[counter].arrival,&arr[counter].runTime,&arr[counter].priority);
        counter++;
    }
    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.
    printf("Choose an algorithm: 1)HPF  2)RR   3)2-cpu FCFS: ");
    int algNum,rrSlot = 0;
    scanf("%d",&algNum);
    if(algNum == 2){

        printf("Enter your time slot: ");
        scanf("%d",&rrSlot);
    }
    // 3. Initiate and create the scheduler and clock processes.
    int clkPid = fork();
    if(clkPid == 0){
        char *clk_argv[] = {"./clk.out", NULL};
        execv("./clk.out",clk_argv);
    }
    int schedularPid = fork();
    if(schedularPid == 0){
    
        char algStr[10], rrStr[10];
        sprintf(algStr, "%d", algNum);
        sprintf(rrStr, "%d", rrSlot);
        char *sch_argv[] = {"./scheduler.out",algStr,rrStr, NULL};
        execv("./scheduler.out",sch_argv);
    }
    // 4. Use this function after creating the clock process to initialize clock
    initClk();
    // To get time use this
    int x = getClk();
    printf("current time is %d\n", x);
    // TODO Generation Main Loop
    // 5. Create a data structure for processes and provide it with its parameters. & 6. Send the information to the scheduler at the appropriate time.
    int process_index = 0;
    key_t msgQueue_key;
    int msgQueue_id;
    msgQueue_key = ftok("clk.c", 10);
    msgQueue_id = msgget(msgQueue_key, 0666 | IPC_CREAT);

    while(process_index != processesNum){
        int now = getClk();
        while(arr[process_index].arrival == now){
            // send in message queue
            arr[process_index].mtype = 1;
            msgsnd(msgQueue_id, &arr[process_index],sizeof(process) - sizeof(long), !IPC_NOWAIT);
            process_index++;
        }
    }
    // 7. Clear clock resources
    destroyClk(true);
}

void clearResources(int signum)
{
    //TODO Clears all resources in case of interruption
}
