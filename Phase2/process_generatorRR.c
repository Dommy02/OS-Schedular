#include "headers.h"
#include "utils.h"

void clearResources(int);
Process *fileReading(int *);
pid_t initiateClk();
pid_t initiateScheduler(int, char[], int, int, int, int);
void creatMessageQueue();
void mainLoop(int);

Process *processes;
int message_queue_id = -1;

int saveMeSemaphore;

int main(int argc, char *argv[])
{
    printf("\n\n");
    signal(SIGINT, clearResources);
    // TODO Initialization
    // 1. Read the input files.

    int number_of_processes = 0;
    processes = fileReading(&number_of_processes);

    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.

    // the algorithms should be HPF , RR , FCFS (2 CPU's)
    // but for now i will run FCFS on 1 CPU
    // char algorithm[10];
    // scanf("%9s", algorithm);
    // printf("%s\n", algorithm);

    char algorithm[10] = "RR";
    int N = 0, M = 0, quantum = atoi(argv[1]), K = atoi(argv[2]);

    saveMeSemaphore = semget(IPC_PRIVATE, 1, 0666 | IPC_CREAT);
    if (saveMeSemaphore == -1)
    {
        perror("semget failed");
        exit(-1);
    }

    union Semun semun;
    semun.val = 0;

    if (semctl(saveMeSemaphore, 0, SETVAL, semun) == -1)
    {
        perror("semctl Failed in Scheduler");
        exit(-1);
    }

    // 3. Initiate and create the scheduler and clock processes.
    // printf("Before initial clock and scheduler\n");
    creatMessageQueue();
    pid_t clk_pid = initiateClk();
    pid_t scheduler_pid = initiateScheduler(number_of_processes, algorithm, N, M, quantum, K);

    // 4. Use this function after creating the clock process to initialize clock
    initClk();
    // To get time use this

    // TODO Generation Main Loop

    // printf("Before the main loop of the process generator\n");
    mainLoop(number_of_processes);
    // printf("after the main loop of the process generator\n");
    // 5. Create a data structure for processes and provide it with its parameters.
    // 6. Send the information to the scheduler at the appropriate time.
    // 7. Clear clock resources

    kill(scheduler_pid, SIGUSR1);
    int status;
    waitpid(scheduler_pid, &status, 0);

    // down(saveMeSemaphore);
    // printf("why am here when the scheduler isn't done ???\n");
    if (WIFEXITED(status))
    {
        destroyClk(true);
    }
}

void clearResources(int signum)
{
    printf("Process Generator Cleared It's Stuff\n");
    // TODO Clears all resources in case of interruption
    if (processes != NULL)
    {
        free(processes);
        processes = NULL;
    }
    if (message_queue_id != -1)
        msgctl(message_queue_id, IPC_RMID, (struct msqid_ds *)0);

    if (saveMeSemaphore != -1)
        semctl(saveMeSemaphore, 0, IPC_RMID);
    exit(0);
}

Process *fileReading(int *number_of_processes)
{
    FILE *file = fopen("processes.txt", "r");
    if (file == NULL)
    {
        perror("Error in Opening Processes File");
        exit(-1);
    }
    char line[256];
    int file_size = 0;
    while (fgets(line, sizeof(line), file) != NULL)
    {
        file_size++;
    }

    if (file_size <= 1)
    {
        // since the first line is a comment
        printf("Error in File Size , No Processes Exist");
        exit(-1);
    }
    rewind(file);

    fgets(line, sizeof(line), file);

    Process *processes = (Process *)malloc((file_size - 1) * sizeof(Process));
    if (processes == NULL)
    {
        printf("Memory allocation failed\n");
        exit(-1);
    }

    for (int i = 0; i < file_size - 1; i++)
    {
        fgets(line, sizeof(line), file);
        sscanf(line, "%d %d %d %d %d %d", &processes[i].id, &processes[i].arrival_time, &processes[i].runtime, &processes[i].priority, &processes[i].base, &processes[i].limit);
        processes[i].m_type = 1;
    }

    fclose(file);
    *number_of_processes = file_size - 1;
    return processes;
}

pid_t initiateClk()
{
    pid_t clk_pid = fork();
    if (clk_pid == -1)
    {
        perror("Error in Forking the Clock");
        exit(-1);
    }
    else if (clk_pid == 0)
    {
        execl("./clk.out", "clk.out", NULL);
        perror("Failed to start clock");
        exit(-1);
    }

    return clk_pid;
}

pid_t initiateScheduler(int num_process, char algorithm[], int N, int M, int quantum, int K)
{
    pid_t scheduler_pid = fork();
    if (scheduler_pid == -1)
    {
        perror("Error in Forking the Scheduler");
        exit(-1);
    }
    else if (scheduler_pid == 0)
    {
        char n_str[10], m_str[10], q_str[10], K_str[10],saveMeSemaphore_str[10];
        sprintf(n_str, "%d", N);
        sprintf(m_str, "%d", M);
        sprintf(q_str, "%d", quantum);
        sprintf(K_str, "%d", K);
        sprintf(saveMeSemaphore_str, "%d", saveMeSemaphore);

        execl("./schedulerRR.out", "schedulerRR.out", algorithm, n_str, m_str, q_str, K_str, saveMeSemaphore_str, NULL);
        perror("Failed to start scheduler");
        exit(-1);
    }

    return scheduler_pid;
}

void creatMessageQueue()
{
    key_t key = ftok("keyfile", 'a');
    if (key == -1)
    {
        perror("ftok failed");
        exit(1);
    }

    message_queue_id = msgget(key, 0666 | IPC_CREAT);

    if (message_queue_id == -1)
    {
        perror("Error in Message Queue Creation");
        exit(-1);
    }
}

void mainLoop(int num)
{
    int p_index = 0;
    int send_val;
    while (p_index < num)
    {
        int currentTime = getClk();
        if (currentTime >= processes[p_index].arrival_time)
        {
            // printf("i am the process generator and the current process is id: %d\n", processes[p_index].id);
            send_val = msgsnd(message_queue_id, &processes[p_index], sizeof(Process), !IPC_NOWAIT);
            if (send_val == -1)
            {
                perror("Error in Message Sending");
                // printf("fuck me if i am here\n");
                exit(-1);
            }
            p_index++;
        }
    }
}