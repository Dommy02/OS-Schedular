#include "headers.h"
#include <vector>
#include <queue>
#include <cstring>
using namespace std;

typedef struct process{
    long mtype;
    int id,arrival,runTime,priority;
    char state;
    int remainigTime,WaitingTime,whichCpu;
    pid_t pid;
} process;

typedef struct node{
    process *curr;
    struct node *next;
} node;

typedef struct WTA_node{
    float curr;
    struct WTA_node *next;
} WTA_node;

process *PCB;
int msgQueue_key_id, totalProcesses = 0;

void Preemtive_HPF();

void RoundRobin(int quantum) {

}

void Cpu2(int n,int m,int numOfProcesses);

int main(int argc, char * argv[])
{
    initClk();
    printf("Scheduler started at t=%d\n", getClk());
    //totalProcesses = atoi(argv[1]);
    /*     Make these lines in each algoritm because we don't use the same structure for NO REASON!!!!!!!
    PCB = (process *)malloc(sizeof(process) * (1+totalProcesses));// 1 indexed
    msgQueue_key_id = msgget(msgQueue_key, 0666 | IPC_CREAT);
    key_t msgQueue_key = ftok("clk.c", 10);
    free(PCB);
    */
    
    
    
    int algNum = atoi(argv[2]);
    switch(algNum) {
        case 1:
            Preemtive_HPF();
            break;
        case 2:
            RoundRobin(atoi(argv[3]));
            break;
        case 3:
            int N = atoi(argv[4]);
            int M = atoi(argv[5]);
            //int totalProcesses = atoi(argv[1]);
            Cpu2(N,M,atoi(argv[6]));
            break;
    }
    
    destroyClk(false);
    return 0;
}

void writeLog(int currentTime, process* p, const char* state) {
    FILE* logFile = fopen("scheduler.log", "a");
    if (logFile == NULL)
        return;
    int timeSpentRunning = p->runTime - p->remainigTime;
    int wait = currentTime - p->arrival - timeSpentRunning;

    fprintf(logFile, "At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d", 
            currentTime, p->id, state, p->arrival, p->runTime, p->remainigTime, wait);

    if (strcmp(state, "finished") == 0) {
        int TA = currentTime - p->arrival;
        double WTA = (double)TA / p->runTime;
        fprintf(logFile, "\tTA\t%d\tWTA\t%.2f", TA, WTA);
    }

    fprintf(logFile, "\n");
    fclose(logFile);
}

struct ComparePriority {
    bool operator()(const process* p1, const process* p2) {
        if (p1->priority != p2->priority)
            return p1->priority > p2->priority;
        if (p1->arrival != p2->arrival)
            return p1->arrival > p2->arrival;
        return p1->id > p2->id;
    }
};

void Preemtive_HPF() {
    process* runningProcess = NULL;
    int finishedCount = 0;
    int lastTime = -1;
    totalProcesses = 0;
    priority_queue<process*, vector<process*>, ComparePriority> readyQueue;

    key_t msgQueue_key = ftok("clk.c", 10);
    msgQueue_key_id = msgget(msgQueue_key, 0666 | IPC_CREAT);
    
    key_t shm_key = ftok("clk.c", 20);
    int shmid = shmget(shm_key, sizeof(SharedData), 0666 | IPC_CREAT);
    SharedData* shared_data = (SharedData*) shmat(shmid, NULL, 0);
    int sem_id = semget(shm_key, 1, 0666 | IPC_CREAT);
    semctl(sem_id, 0, SETVAL, 1);

    key_t sync_key = ftok("clk.c", 55);
    int sync_sem = semget(sync_key, 1, 0666 | IPC_CREAT);
    semctl(sync_sem, 0, SETVAL, 1);

    shared_data->remainingTime = -1;
    shared_data->finished = false;
    
    key_t sharedMemKey = ftok("clk.c", 90);
    int sendingData = shmget(sharedMemKey, sizeof(generator_scheduler), 0666 | IPC_CREAT);
    generator_scheduler *schedulerData = (generator_scheduler*) shmat(sendingData, NULL, 0);

    FILE *f = fopen("scheduler.log", "w");
    fprintf(f, "#At\ttime\tx\tprocess\ty\tstate\tarr\tw\ttotal\tz\tremain\ty\twait\tk\n");
    fclose(f);

    bool didWait = false;

    int total_execution_time = 0;
    int total_waiting_time = 0;
    double total_WTA = 0.0;
    vector<double> WTAs_list;

    while (!(schedulerData->IamFinished) || finishedCount < totalProcesses) {
        process msg;
        while (msgrcv(msgQueue_key_id, &msg, sizeof(process) - sizeof(long), 0, IPC_NOWAIT) != -1) {
            msg.state = 'N';
            readyQueue.push(new process(msg));
            totalProcesses++;
        }

        int current_time = getClk();

        if (runningProcess) {
            sem_wait(sem_id);
            sem_signal(sem_id);
            int status;
            if (shared_data->remainingTime == 0) {
                waitpid(runningProcess->pid, &status, 0);
                runningProcess->remainigTime = 0;
                writeLog(current_time, runningProcess, "finished");

                int TA = current_time - runningProcess->arrival;
                double WTA = (double)TA / runningProcess->runTime;
                int wait = TA - runningProcess->runTime;
                total_execution_time += runningProcess->runTime;
                total_waiting_time += wait;
                total_WTA += WTA;
                WTAs_list.push_back(WTA);

                delete runningProcess;
                runningProcess = NULL;
                shared_data->remainingTime = -1;
                finishedCount++;
                lastTime = current_time;
            }
            else {
                if (!readyQueue.empty() && readyQueue.top()->priority < runningProcess->priority) {
                    if (!didWait) {
                        didWait = true;
                        kill(runningProcess->pid, SIGUSR1);
                        sem_wait(sync_sem);
                        sem_signal(sync_sem);
                        if (shared_data->remainingTime == 0)
                            continue;
                    }
                    else
                        didWait = false;
                    kill(runningProcess->pid, SIGSTOP);
                    runningProcess->remainigTime = shared_data->remainingTime;
                    runningProcess->state = 'S';
                    writeLog(current_time, runningProcess, "stopped");
                    readyQueue.push(runningProcess);
                    runningProcess = NULL;
                    semctl(sem_id, 0, SETVAL, 1);
                    lastTime = current_time;
                }
            }
        }

        if (lastTime == current_time)
            continue;

        if (!runningProcess && !readyQueue.empty()) {
            runningProcess = readyQueue.top();
            readyQueue.pop();
            if (runningProcess->state == 'N') {
                pid_t pid = fork();
                if (pid == 0) {
                    char idStr[16], remTimeStr[16], shmidStr[16], semidStr[16], syncidStr[16];;
                    sprintf(idStr, "%d", runningProcess->id);
                    sprintf(remTimeStr, "%d", runningProcess->remainigTime);
                    sprintf(shmidStr, "%d", shmid);
                    sprintf(semidStr, "%d", sem_id);
                    sprintf(syncidStr, "%d", sync_sem);
                    execl("./process.out", "./process.out", idStr, remTimeStr, shmidStr, semidStr, syncidStr, (char*)NULL);
                    perror("exec failed");
                    exit(1);
                }
                runningProcess->pid = pid;
                runningProcess->state = 'R';
                writeLog(current_time, runningProcess, "started");    
            } 
            else if (runningProcess->state == 'S') {
                kill(runningProcess->pid, SIGCONT);
                runningProcess->state = 'R';
                writeLog(current_time, runningProcess, "resumed");    
            }
        }
    }

    printf("HPF Scheduler finished all processes at t=%d\n", getClk());

    int finish_time = getClk();
    double cpu_utilization = ((double)total_execution_time / finish_time) * 100.0;
    double avg_WTA = total_WTA / finishedCount;
    double avg_Waiting = (double)total_waiting_time / finishedCount;
    
    double sum_squared_diff = 0.0;
    for (double wta : WTAs_list) {
        sum_squared_diff += (wta - avg_WTA) * (wta - avg_WTA);
    }
    double std_WTA = sqrt(sum_squared_diff / finishedCount);
    
    FILE *perfFile = fopen("scheduler.perf", "w");
    if (perfFile != NULL) {
        fprintf(perfFile, "CPU utilization = %.2f%%\n", cpu_utilization);
        fprintf(perfFile, "Avg WTA = %.2f\n", avg_WTA);
        fprintf(perfFile, "Avg Waiting = %.2f\n", avg_Waiting);
        fprintf(perfFile, "Std WTA = %.2f\n", std_WTA);
        fclose(perfFile);
    }

    shmdt(shared_data);
    shmdt(schedulerData);
}

void Cpu2(int n,int m,int sharedMem_key){
    int N = n;
    int M = m;
    bool stoped = false;
    int totalProcesses = 0;
    int waitingTime = 0;
    initClk();
    int now = getClk();
    
    printf("Scheduler started at t=%d\n", now);

    // Main queue that stores all received processes in arrival order.
    node* Head_CPU1 = NULL,*Tail_CPU1 = NULL;
    node* Head_CPU2 = NULL,*Tail_CPU2 = NULL;
    node *p1 = NULL, *p2 = NULL;
    WTA_node* WTA_head1 = NULL,*WTA_head2 = NULL;
    WTA_node* WTA_tail1 = NULL,*WTA_tail2 = NULL;
    int Queue1_load = 0 , Queue2_load = 0;
    int numProcessesEntered_CPU1 = 0 , numProcessesEntered_CPU2 = 0;
    int allRunningTime1 = 0, allRunningTime2 = 0;
    float all_WTA1 = 0, all_WTA2 = 0;
    int all_Waiting1 = 0, all_Waiting2 = 0;
    int endTime1 = 0 , endTime2 = 0;
    
    int numAllProcesses = 0;

    int receivedCount = 0, finishedCount = 0;
    bool switch1 = false, switch2 = false;
    int TimeToBaclFromSwitch_cpu1 = 0,TimeToBaclFromSwitch_cpu2 = 0;

    key_t msgQueue_key = ftok("clk.c", 10);
    int msgQueue_key_id = msgget(msgQueue_key, 0666 | IPC_CREAT);
    

    bool cpuBusy1 = false;
    bool cpuBusy2 = false;
    int runningId1 = -1,runningId2 = -1;
    pid_t runningPid1 = -1;
    pid_t runningPid2 = -1;
    // shared Memory between Scheduler and process generator
    generator_scheduler *schedulerData = (generator_scheduler*) shmat(sharedMem_key, NULL, 0);//
    int sem_id = schedulerData->sem_id;
    // shared Memory between process and Scheduler
    key_t sharedMem_key_cpu1 = ftok("clk.c", 20);
    key_t sharedMem_key_cpu2 = ftok("clk.c", 30);
    // Create shared memory
    int sharedMem_id_cpu1 = shmget(sharedMem_key_cpu1, sizeof(SharedData), 0666 | IPC_CREAT);
    int sharedMem_id_cpu2 = shmget(sharedMem_key_cpu2, sizeof(SharedData), 0666 | IPC_CREAT);
    SharedData *CPU1_data = (SharedData*) shmat(sharedMem_id_cpu1, NULL, 0);
    SharedData *CPU2_data = (SharedData*) shmat(sharedMem_id_cpu2, NULL, 0);
    CPU1_data->busy = false;
    CPU2_data->busy = false;
    CPU1_data->finished = false;
    CPU2_data->finished = false;

    int CPU1_sem_id = semget(sharedMem_key_cpu1, 1, 0666 | IPC_CREAT);
    int CPU2_sem_id = semget(sharedMem_key_cpu2, 1, 0666 | IPC_CREAT);
    semctl(CPU1_sem_id, 0, SETVAL, 1);
    semctl(CPU2_sem_id, 0, SETVAL, 1);

    int checking_time = 0 + N;
    printf("checkin Time = %d\n",checking_time);
    int returnFromSteal = checking_time + M + 1;
    printf("return from steal Time = %d\n",returnFromSteal);
    
    // to get the size of generator - scheduler queue
    struct msqid_ds buf;
    while (!schedulerData->IamFinished || finishedCount != totalProcesses)
    {
        // Receive all currently available processes from bus
        sem_wait(schedulerData->sem_id);
        bool sendingNow = schedulerData->IamSendingNow;
        sem_signal(schedulerData->sem_id);
        msgctl(msgQueue_key_id, IPC_STAT, &buf);
        while (sendingNow == true || buf.msg_qnum != 0 ) {    
                process messageBuff;
                msgrcv(msgQueue_key_id, &messageBuff, sizeof(process) - sizeof(long), 0, !IPC_NOWAIT);
                receivedCount++;
                totalProcesses++;
                // to renew after receive
                msgctl(msgQueue_key_id, IPC_STAT, &buf);
                sem_wait(schedulerData->sem_id);
                sendingNow = schedulerData->IamSendingNow;
                sem_signal(schedulerData->sem_id);
                //enqueue in ready queues
                if(Queue1_load <= Queue2_load){
                    if(Queue1_load == 0){
                        Head_CPU1 = (node*)malloc(sizeof(node));
                        Tail_CPU1 = Head_CPU1;
                        Tail_CPU1->next = NULL;
                        Tail_CPU1->curr = (process*)malloc(sizeof(process));
                        Tail_CPU1->curr->id = messageBuff.id;
                        Tail_CPU1->curr->remainigTime = messageBuff.remainigTime;
                        Tail_CPU1->curr->runTime = messageBuff.runTime;
                        Tail_CPU1->curr->arrival = messageBuff.arrival;
                        Tail_CPU1->curr->whichCpu = 1;
                    }
                    else{
                        Tail_CPU1->next = (node*)malloc(sizeof(node));
                        Tail_CPU1 = Tail_CPU1->next;
                        Tail_CPU1->next = NULL;
                        Tail_CPU1->curr = (process*)malloc(sizeof(process));
                        Tail_CPU1->curr->id = messageBuff.id;
                        Tail_CPU1->curr->remainigTime = messageBuff.remainigTime;
                        Tail_CPU1->curr->runTime = messageBuff.runTime;
                        Tail_CPU1->curr->arrival = messageBuff.arrival;
                        Tail_CPU1->curr->whichCpu = 1;
                    }
                    Queue1_load++;
                    numProcessesEntered_CPU1++;
                }
                else{
                    if(Queue2_load == 0){
                        Head_CPU2 = (node*)malloc(sizeof(node));
                        Tail_CPU2 = Head_CPU2;
                        Tail_CPU2->next = NULL;
                        Tail_CPU2->curr = (process*)malloc(sizeof(process));
                        Tail_CPU2->curr->id = messageBuff.id;
                        Tail_CPU2->curr->remainigTime = messageBuff.remainigTime;
                        Tail_CPU2->curr->runTime = messageBuff.runTime;
                        Tail_CPU2->curr->arrival = messageBuff.arrival;
                        Tail_CPU2->curr->whichCpu = 2;
                    }
                    else{
                        Tail_CPU2->next = (node*)malloc(sizeof(node));
                        Tail_CPU2 = Tail_CPU2->next;
                        Tail_CPU2->next = NULL;
                        Tail_CPU2->curr = (process*)malloc(sizeof(process));
                        Tail_CPU2->curr->id = messageBuff.id;
                        Tail_CPU2->curr->remainigTime = messageBuff.remainigTime;
                        Tail_CPU2->curr->runTime = messageBuff.runTime;
                        Tail_CPU2->curr->arrival = messageBuff.arrival;
                        Tail_CPU2->curr->whichCpu = 2;
                    }
                    Queue2_load++;  
                    numProcessesEntered_CPU2++; 
            }
        }

        // check for stealing
        if(!(!cpuBusy1 && Queue1_load != 0) && !(!cpuBusy2 && Queue2_load != 0)){
            now = getClk();
            sem_wait(schedulerData->sem_id);
            sendingNow = schedulerData->IamSendingNow;
            sem_signal(schedulerData->sem_id);
            struct msqid_ds buf1;
            msgctl(msgQueue_key_id, IPC_STAT, &buf1);
            if(checking_time == now && sendingNow == false && buf1.msg_qnum == 0 ){
                printf("now is T%d and checking time = T%d and state is %d\n",now,checking_time,stoped);
                checking_time = now+N;
                if(stoped == false)
                {    int remainingTime1 = 0, remainingTime2 = 0;
                    sem_wait(CPU1_sem_id);
                    remainingTime1 += CPU1_data->remainingTime;
                    sem_signal(CPU1_sem_id);
                    printf("remaining in cpu1 = %d\n",remainingTime1);
                    sem_wait(CPU2_sem_id);
                    remainingTime2 += CPU2_data->remainingTime;
                    sem_signal(CPU2_sem_id);
                    printf("remaining in cpu2 = %d\n",remainingTime2);
                    node* head1_cpy = Head_CPU1;
                    node* head2_cpy = Head_CPU2;
                    while(head1_cpy != NULL){
                        remainingTime1 += head1_cpy->curr->remainigTime;
                        printf("remaining1 is = %d, P%d\n",remainingTime1,head1_cpy->curr->id);
                        head1_cpy = head1_cpy->next;
                    }
                    
                    while(head2_cpy != NULL){
                        remainingTime2 += head2_cpy->curr->remainigTime;
                        printf("remaining2 is = %d, P%d\n",remainingTime2,head2_cpy->curr->id);
                        head2_cpy = head2_cpy->next;
                    }
                    
                    printf("M = %d and diff = %d\n",M,abs(remainingTime2 - remainingTime1));
                    if(abs(remainingTime2 - remainingTime1) > M){
                        //stealing
                        returnFromSteal = getClk() + M + 1;
                        if(remainingTime2 < remainingTime1){
                            if(Queue1_load != 0){
                                if(runningPid1 != -1)
                                    kill(runningPid1, SIGSTOP);
                                if(runningPid2 != -1)
                                    kill(runningPid2, SIGSTOP);
                                //
                                node* newTail = Head_CPU1;
                                if(Queue1_load == 1)
                                    newTail = NULL;// for one node in queue
                                else{
                                    while(newTail && newTail->next != Tail_CPU1)
                                        newTail = newTail->next;
                                }
                                // pass the node to queue 2
                                if(Queue2_load == 0){
                                    Tail_CPU2 = Tail_CPU1;
                                    Head_CPU2 = Tail_CPU2;
                                    Tail_CPU2->next = NULL;
                                }
                                else{
                                    Tail_CPU2->next = Tail_CPU1;
                                    Tail_CPU2 = Tail_CPU2->next;
                                    Tail_CPU2->next = NULL;
                                }
                                Tail_CPU2->curr->whichCpu = 2;
                                // handling the new tail cpu 1
                                Tail_CPU1 = newTail;
                                Queue1_load--;
                                Queue2_load++;
                                if(Queue1_load != 0)
                                    Tail_CPU1->next = NULL;
                                else{
                                    Tail_CPU1 = Head_CPU1 = NULL;
                                }
                                numProcessesEntered_CPU1--;
                                numProcessesEntered_CPU2++;
                                stoped = true;
                                printf("P%d is stealed to cpu%d and state now is %d\n",Tail_CPU2->curr->id,Tail_CPU2->curr->whichCpu,stoped);
                                if(p1 != NULL)
                                    p1->curr->WaitingTime +=3;
                                if(p2 != NULL)
                                    p2->curr->WaitingTime +=3;
                                //file things
                                FILE *fp = fopen("scheduler_1.log", "a");
                                fprintf(fp, "At\ttime\t%d\tprocess\t%d\twas\tstolen\n",getClk(),Tail_CPU2->curr->id);
                                fclose(fp);
                                //
                            }
                        }
                        else{
                            if(Queue2_load != 0){
                                if(runningPid1 != -1)
                                    kill(runningPid1, SIGSTOP);
                                if(runningPid2 != -1)
                                    kill(runningPid2, SIGSTOP);
                                //
                                node* newTail = Head_CPU2;
                                if(Queue2_load == 1)
                                    newTail = NULL;// for one node in queue
                                else{
                                    while(newTail && newTail->next != Tail_CPU2)
                                        newTail = newTail->next;
                                }
                                // pass the node to queue 1
                                if(Queue1_load == 0){
                                    Tail_CPU1 = Tail_CPU2;
                                    Head_CPU1 = Tail_CPU1;
                                    Tail_CPU1->next = NULL;
                                }
                                else{
                                    Tail_CPU1->next = Tail_CPU2;
                                    Tail_CPU1 = Tail_CPU1->next;
                                    Tail_CPU1->next = NULL;
                                }
                                Tail_CPU1->curr->whichCpu = 1;
                                // handling the new tail cpu 2
                                Queue1_load++;
                                Queue2_load--;
                                Tail_CPU2 = newTail;
                                if(Queue2_load != 0){
                                    Tail_CPU2->next = NULL;
                                }
                                else{
                                    Tail_CPU2 = Head_CPU2 = NULL;
                                }
                                numProcessesEntered_CPU1++;
                                numProcessesEntered_CPU2--;
                                stoped = true;
                                printf("P%d is stealed to cpu%d and state is %d\n",Tail_CPU1->curr->id,Tail_CPU1->curr->whichCpu,stoped);
                                if(p1 != NULL)
                                    p1->curr->WaitingTime +=3;
                                if(p2 != NULL)
                                    p2->curr->WaitingTime +=3;
                                //file things
                                FILE *fp = fopen("scheduler_2.log", "a");
                                fprintf(fp, "At\ttime\t%d\tprocess\t%d\twas\tstolen\n",getClk(),Tail_CPU1->curr->id);
                                fclose(fp);
                                //
                            }
                        }
                    }
                }
            }
            else if(stoped && returnFromSteal == getClk()){
                printf("return from steal penalty at %d\n",returnFromSteal);
                stoped = false;
                printf("T%d stoped state is %d\n",now,stoped);
                kill(runningPid1, SIGCONT);
                kill(runningPid2, SIGCONT);
            }
        }

        // cpu1
        if (!cpuBusy1 && Queue1_load != 0)
        {
            if(switch1 == false){
                p1 = Head_CPU1;
                Head_CPU1 = Head_CPU1->next;
                runningId1 = p1->curr->id;
                CPU1_data->finished      = false;
                CPU1_data->busy          = true;
                CPU1_data->remainingTime = p1->curr->remainigTime;
                p1->curr->WaitingTime = getClk() - p1->curr->arrival;
                cpuBusy1                 = true;

                pid_t pid = fork();
                if (pid == 0)
                {
                    char ProcessId_str[16], remainingTime_str[16],sharedMem_id_cpu1_str[16],CPU1_sem_id_str[16];
                    sprintf(ProcessId_str, "%d", p1->curr->id);
                    sprintf(remainingTime_str, "%d", p1->curr->remainigTime);
                    sprintf(sharedMem_id_cpu1_str, "%d", sharedMem_id_cpu1);
                    sprintf(CPU1_sem_id_str, "%d", CPU1_sem_id);
                    
                    char* process_argv[] = {"./process.out",ProcessId_str,remainingTime_str,sharedMem_id_cpu1_str,CPU1_sem_id_str,NULL};
                    execv("./process.out",process_argv);
                    _exit(0);
                }
                runningPid1 = pid;
                Queue1_load--;
                if(Queue1_load == 0){
                    Tail_CPU1 = Head_CPU1 = NULL;//reached the end
                }
                printf("Started P%d at t=%d on CPU 1\n", runningId1, getClk());
                // File things
                FILE *fp = fopen("scheduler_1.log", "a");
                if(runningId1 != -1)
                fprintf(fp, "At\ttime\t%d\tprocess\t%d\tstarted\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\n",getClk(),runningId1,p1->curr->arrival,p1->curr->runTime,p1->curr->remainigTime,p1->curr->WaitingTime);
                fclose(fp);
                p1->curr->WaitingTime = getClk() - p1->curr->arrival;
                allRunningTime1 += p1->curr->remainigTime;
            }
            else if(TimeToBaclFromSwitch_cpu1 == getClk()){
                switch1 = false;
                printf("Back to work normally p2\n");
            }
        }
        // cpu 2
        if (!cpuBusy2 && Queue2_load != 0)
        {
            if(switch2 == false){
                
                p2 = Head_CPU2;
                Head_CPU2 = Head_CPU2->next;
                runningId2 = p2->curr->id;

                CPU2_data->finished      = false;
                CPU2_data->busy          = true;
                CPU2_data->remainingTime = p2->curr->remainigTime;
                cpuBusy2                 = true;
                p2->curr->WaitingTime = getClk() - p2->curr->arrival;
                pid_t pid = fork();
                if (pid == 0)
                {
                    char ProcessId_str[16], remainingTime_str[16],sharedMem_id_cpu2_str[16],CPU2_sem_id_str[16];
                    sprintf(ProcessId_str, "%d", p2->curr->id);
                    sprintf(remainingTime_str, "%d", p2->curr->remainigTime);
                    sprintf(sharedMem_id_cpu2_str, "%d", sharedMem_id_cpu2);
                    sprintf(CPU2_sem_id_str, "%d", CPU2_sem_id);
                    char* process_argv[] = {"./process.out",ProcessId_str,remainingTime_str,sharedMem_id_cpu2_str,CPU2_sem_id_str,NULL};
                    execv("./process.out",process_argv);
                    _exit(0);
                }
                runningPid2 = pid;
                Queue2_load--;
                if(Queue2_load == 0){
                    Tail_CPU2 = Head_CPU2 = NULL;//reached the end
                }
                printf("Started P%d at t=%d on CPU 2\n", runningId2, getClk());
                // file things
                FILE *fp = fopen("scheduler_2.log", "a");
                if(runningId2 != -1)
                fprintf(fp, "At\ttime\t%d\tprocess\t%d\tstarted\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\n",getClk(),runningId2,p2->curr->arrival,p2->curr->runTime,p2->curr->remainigTime,p2->curr->WaitingTime);
                fclose(fp);
                p2->curr->WaitingTime = getClk() - p2->curr->arrival;
                allRunningTime2 += p2->curr->remainigTime;
            }
            else if(TimeToBaclFromSwitch_cpu2 == getClk()){
                switch2 = false;
                printf("Back to work normally p2\n");
            }
        }

        // CPU1
        if (cpuBusy1)
        {
            int status;
            if (waitpid(runningPid1, &status, WNOHANG) > 0)
            {
                
                printf("Finished P%d at t=%d on CPU 1 | remaining=%d finished=%d\n",runningId1, getClk(),CPU1_data->remainingTime, CPU1_data->finished);
                //file things
                FILE *fp = fopen("scheduler_1.log", "a");
                fprintf(fp, "At\ttime\t%d\tprocess\t%d\tfinished\tarr\t%d\ttotal\t%d\tremaining\t%d\twait\t%d\tTA\t%d\tWAT\t%f\n",getClk(),runningId1,p1->curr->arrival,p1->curr->remainigTime,0,p1->curr->WaitingTime,getClk()-p1->curr->arrival,(getClk()-p1->curr->arrival)*1.0/p1->curr->remainigTime);
                fclose(fp);
                //
                all_Waiting1 += p1->curr->WaitingTime;
                if(WTA_head1 == NULL){
                    WTA_head1 = (WTA_node*)malloc(sizeof(WTA_node));
                    WTA_tail1 = WTA_head1;
                    WTA_tail1->curr = (getClk()-p1->curr->arrival)*1.0/p1->curr->remainigTime;
                    WTA_tail1->next = NULL;
                }
                else{
                    WTA_tail1->next = (WTA_node*)malloc(sizeof(WTA_node));
                    WTA_tail1 = WTA_tail1->next;
                    WTA_tail1->curr = (getClk()-p1->curr->arrival)*1.0/p1->curr->remainigTime;
                    WTA_tail1->next = NULL;
                }
                all_WTA1 += (getClk()-p1->curr->arrival)*1.0/p1->curr->remainigTime;
                finishedCount++;
                cpuBusy1    = false;
                switch1 = true;
                TimeToBaclFromSwitch_cpu1 = getClk() + 1;
                runningPid1 = -1;
                runningId1  = -1;
                printf("total num of processes: %d received num = %d, finished: %d\n",totalProcesses, receivedCount, finishedCount);
                free(p1->curr);
                free(p1);
                p1 = NULL;
                endTime1 = getClk();
            }
        }
        // CPU2
        if (cpuBusy2)
        {
            int status;
            if (waitpid(runningPid2, &status, WNOHANG) > 0)
            {
                printf("Finished P%d at t=%d on CPU 2 | remaining=%d finished=%d\n",runningId2, getClk(),CPU2_data->remainingTime, CPU2_data->finished);
                //file things
                FILE *fp = fopen("scheduler_2.log", "a");
                fprintf(fp, "At\ttime\t%d\tprocess\t%d\tfinished\tarr\t%d\ttotal\t%d\tremaining\t%d\twait\t%d\tTA\t%d\tWAT\t%f\n",getClk(),runningId2,p2->curr->arrival,p2->curr->remainigTime,0,p2->curr->WaitingTime,getClk()-p2->curr->arrival,(getClk()-p2->curr->arrival)*1.0/p2->curr->remainigTime);
                fclose(fp);
                //
                all_Waiting2 += p2->curr->WaitingTime;
                if(WTA_head2 == NULL){
                    WTA_head2 = (WTA_node*)malloc(sizeof(WTA_node));
                    WTA_tail2 = WTA_head2;
                    WTA_tail2->curr = (getClk()-p2->curr->arrival)*1.0/p2->curr->remainigTime;
                    WTA_tail2->next = NULL;
                }
                else{
                    WTA_tail2->next = (WTA_node*)malloc(sizeof(WTA_node));
                    WTA_tail2 = WTA_tail2->next;
                    WTA_tail2->curr = (getClk()-p2->curr->arrival)*1.0/p2->curr->remainigTime;
                    WTA_tail2->next = NULL;
                }
                all_WTA2 += (getClk()-p2->curr->arrival)*1.0/p2->curr->remainigTime;
                finishedCount++;
                cpuBusy2    = false;
                switch2 = true;
                TimeToBaclFromSwitch_cpu2 = getClk() + 1;
                runningPid2 = -1;
                runningId2  = -1;
                printf("total num of processes: %d received num = %d, finished: %d\n",totalProcesses, receivedCount, finishedCount);
                free(p2->curr);
                free(p2);
                p2 = NULL;
                endTime2 = getClk();
            }
        }

        
    }
    printf("number entered cpu1 : %d, waiting1 = %d\n",numProcessesEntered_CPU1,all_Waiting1);
    printf("number entered cpu2 : %d, waiting2 = %d\n",numProcessesEntered_CPU2,all_Waiting2);
    float avgWTA1 = all_WTA1/numProcessesEntered_CPU1, avgWTA2 = all_WTA2/numProcessesEntered_CPU2;
    float sum1 = 0 , sum2 = 0;
    WTA_node* cpy1 = WTA_head1, *cpy2 = WTA_head2;
    while(WTA_head1!=NULL){
        sum1 += (WTA_head1->curr - avgWTA1) * (WTA_head1->curr - avgWTA1);
        WTA_head1 = WTA_head1->next;
        free(cpy1);
        cpy1 = WTA_head1;
    }
    while(WTA_head2!=NULL){
        sum2 += (WTA_head2->curr - avgWTA2) * (WTA_head2->curr - avgWTA2);
        WTA_head2 = WTA_head2->next;
        free(cpy2);
        cpy2 = WTA_head2;
    }
    float std1 = sqrt(sum1/numProcessesEntered_CPU1), std2 = sqrt(sum2/numProcessesEntered_CPU2);
    //performance files
    FILE *fp1 = fopen("scheduler_1.perf", "a");
    fprintf(fp1,"CPU utilization = %f\n",round(allRunningTime1*1.0/endTime1 * 100 *100)/100);
    fprintf(fp1,"Avg WTA = %f\n",round(all_WTA1*1.0 / numProcessesEntered_CPU1 * 100)/100);
    fprintf(fp1,"Avg Waiting = %f\n",round(all_Waiting1*1.0/numProcessesEntered_CPU1 * 100) / 100);
    fprintf(fp1,"Std WTA = %f\n",round(std1*100)/100);
    fclose(fp1);
    FILE *fp2 = fopen("scheduler_2.perf", "a");
    fprintf(fp2,"CPU utilization = %f\n",round(allRunningTime2*1.0/endTime2 * 100 *100)/100);
    fprintf(fp2,"Avg WTA = %f\n",round(all_WTA2*1.0 / numProcessesEntered_CPU2 * 100)/100);
    fprintf(fp2,"Avg Waiting = %f\n",round(all_Waiting2*1.0/numProcessesEntered_CPU2 * 100) / 100);
    fprintf(fp2,"Std WTA = %f\n",round(std2*100)/100);
    fclose(fp2);
    //
    printf("I am scheduler and I am finished\n");
    shmdt(CPU1_data);
    shmctl(sharedMem_id_cpu1, 0,IPC_RMID);
    semctl(CPU1_sem_id, 0, IPC_RMID);
    shmdt(CPU2_data);
    shmctl(sharedMem_id_cpu2, IPC_RMID, NULL);
    semctl(CPU2_sem_id, 0, IPC_RMID);
    shmdt(schedulerData);
    //destroyClk(false);
    printf("%d\n",getpid());
    exit(100);
}