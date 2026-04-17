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
            int totalProcesses = atoi(argv[1]);
            Cpu2(N,M,totalProcesses);
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

void Cpu2(int n,int m,int numOfProcesses){
    int N = n;
    int M = m;
    bool stoped = false;
    int totalProcesses = numOfProcesses;

    initClk();
    int now = getClk();
    
    printf("Scheduler started at t=%d\n", now);

    // Main queue that stores all received processes in arrival order.
    process *allProcesses = (process *)malloc(sizeof(process) * totalProcesses);

    // Two CPU queues store indices into allProcesses.
    int *cpu1IdxQ = (int *)malloc(sizeof(int) * totalProcesses);
    int *cpu2IdxQ = (int *)malloc(sizeof(int) * totalProcesses);

    int globalTail = 0;
    int head_cpu1 = 0, tail_cpu1 = 0;
    int head_cpu2 = 0, tail_cpu2 = 0;
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
    semctl(CPU1_sem_id, 0, SETVAL, 0);
    semctl(CPU2_sem_id, 0, SETVAL, 0);

    int checking_time = now + N;
    printf("checkin Time = %d\n",checking_time);
    while (finishedCount < totalProcesses)
    {
        // Receive all currently available processes from bus
        process messageBuff;
        while (msgrcv(msgQueue_key_id, &messageBuff, sizeof(process) - sizeof(long), 1, IPC_NOWAIT) != -1)
        {
            if (true)
            {    
                if (globalTail < totalProcesses)
                {
                    int newProcessIndex = globalTail;
                    allProcesses[globalTail++] = messageBuff;

                    int cpu1Load = tail_cpu1 - head_cpu1;
                    int cpu2Load = tail_cpu2 - head_cpu2;

                    // Least-loaded queue policy; tie goes to CPU #1.
                    if (cpu1Load <= cpu2Load)
                    {
                        cpu1IdxQ[tail_cpu1++] = newProcessIndex;
                        allProcesses[newProcessIndex].whichCpu = 1;
                    }
                    else
                    {
                        cpu2IdxQ[tail_cpu2++] = newProcessIndex;
                        allProcesses[newProcessIndex].whichCpu = 2;
                    }

                    printf("Enqueued P%d at t=%d (runtime=%d) in CPU number %d\n",allProcesses[newProcessIndex].id,getClk(),allProcesses[newProcessIndex].runTime,allProcesses[newProcessIndex].whichCpu);
                }
                receivedCount++;
            }
        }
        // check for stealing
        now = getClk();
        if(checking_time == now){
            printf("now is T%d and checking time = T%d and state is %d\n",now,checking_time,stoped);
            checking_time = now+N;
            if(stoped == false)
            {    int remainingTime1 = 0, remainingTime2 = 0;
                remainingTime1 += CPU1_data->remainingTime;
                printf("remaining in cpu1 = %d\n",remainingTime1);
                remainingTime2 += CPU2_data->remainingTime;
                printf("remaining in cpu2 = %d\n",remainingTime2);
                int head1_cpy = head_cpu1;
                int head2_cpy = head_cpu2;
                while(head1_cpy != tail_cpu1){
                    remainingTime1 += allProcesses[cpu1IdxQ[head1_cpy++]].remainigTime;
                    printf("remaining1 is = %d\n",remainingTime1);
                }
                
                while(head2_cpy != tail_cpu2){
                    remainingTime2 += allProcesses[cpu2IdxQ[head2_cpy++]].remainigTime;
                    printf("remaining2 is = %d\n",remainingTime2);
                }
                
                printf("M = %d and diff = %d\n",M,abs(remainingTime2 - remainingTime1));
                if(abs(remainingTime2 - remainingTime1) > M){
                            
                    //stealing
                    if(remainingTime2 < remainingTime1){
                        if(tail_cpu1 != head_cpu1){
                            if(runningPid1 != -1)
                                kill(runningPid1, SIGSTOP);
                            if(runningPid2 != -1)
                                kill(runningPid2, SIGSTOP);
                            //
                            int stealed = cpu1IdxQ[--tail_cpu1];
                            cpu2IdxQ[tail_cpu2++] = stealed;
                            allProcesses[cpu2IdxQ[tail_cpu2-1 ]].whichCpu = 2; 
                            stoped = true;
                            printf("P%d is stealed to cpu%d and state now is %d\n",allProcesses[cpu2IdxQ[tail_cpu2-1 ]].id,allProcesses[cpu2IdxQ[tail_cpu2-1 ]].whichCpu,stoped);
                            //file things
                            FILE *fp = fopen("scheduler_1.log", "a");
                            fprintf(fp, "At\ttime\t%d\tprocess\t%d\twas\tstolen\n",getClk(),allProcesses[cpu2IdxQ[tail_cpu2-1 ]].id);
                            fclose(fp);
                            //
                        }
                    }
                    else{
                        if(tail_cpu2 != head_cpu2){
                            if(runningPid1 != -1)
                                kill(runningPid1, SIGSTOP);
                            if(runningPid2 != -1)
                                kill(runningPid2, SIGSTOP);
                            //
                            int stealed = cpu2IdxQ[--tail_cpu2];
                            cpu1IdxQ[tail_cpu1++] = stealed;
                            allProcesses[cpu1IdxQ[tail_cpu1-1 ]].whichCpu = 1; 
                            printf("P%d is stealed to cpu%d and state is %d\n",allProcesses[cpu1IdxQ[tail_cpu1-1 ]].id,allProcesses[cpu1IdxQ[tail_cpu1-1 ]].whichCpu,stoped);
                            //file things
                            FILE *fp = fopen("scheduler_2.log", "a");
                            fprintf(fp, "At\ttime\t%d\tprocess\t%d\twas\tstolen\n",getClk(),allProcesses[cpu1IdxQ[tail_cpu1-1 ]].id);
                            fclose(fp);
                            //
                        }
                    }
                }
            }
            else{
                stoped = false;
                printf("T%d stoped state is %d\n",now,stoped);
                kill(runningPid1, SIGCONT);
                kill(runningPid2, SIGCONT);
            }
        }
        // cpu1
        if (!cpuBusy1 && head_cpu1 < tail_cpu1)
        {
            if(switch1 == false){
                int pIdx = cpu1IdxQ[head_cpu1++];
                process p = allProcesses[pIdx];
                runningId1 = p.id;

                CPU1_data->finished      = false;
                CPU1_data->busy          = true;
                CPU1_data->remainingTime = p.remainigTime;
                cpuBusy1                 = true;

                pid_t pid = fork();
                if (pid == 0)
                {
                    char ProcessId_str[16], remainingTime_str[16],sharedMem_id_cpu1_str[16],CPU1_sem_id_str[16];
                    sprintf(ProcessId_str, "%d", p.id);
                    sprintf(remainingTime_str, "%d", p.remainigTime);
                    sprintf(sharedMem_id_cpu1_str, "%d", sharedMem_id_cpu1);
                    sprintf(CPU1_sem_id_str, "%d", CPU1_sem_id);
                    
                    char* process_argv[] = {(char*)"./process.out",ProcessId_str,remainingTime_str,sharedMem_id_cpu1_str,CPU1_sem_id_str,NULL};
                    execv("./process.out",process_argv);
                    _exit(0);
                }
                runningPid1 = pid;
                printf("Started P%d at t=%d on CPU 1\n", runningId1, getClk());
                // File things
                FILE *fp = fopen("scheduler_1.log", "a");
                if(runningId1 != -1)
                    fprintf(fp, "At\ttime\t%d\tprocess\t%d\tstarted\n",getClk(),runningId1);
                fclose(fp);
            }
            else if(TimeToBaclFromSwitch_cpu1 == getClk()){
                switch1 = false;
                printf("Back to work normally p2\n");
            }
        }
        // cpu 2
        if (!cpuBusy2 && head_cpu2 < tail_cpu2)
        {
            if(switch2 == false){
                int pIdx = cpu2IdxQ[head_cpu2++];
                process p = allProcesses[pIdx];
                runningId2 = p.id;

                CPU2_data->finished      = false;
                CPU2_data->busy          = true;
                CPU2_data->remainingTime = p.remainigTime;
                cpuBusy2                 = true;

                pid_t pid = fork();
                if (pid == 0)
                {
                    char ProcessId_str[16], remainingTime_str[16],sharedMem_id_cpu2_str[16],CPU2_sem_id_str[16];
                    sprintf(ProcessId_str, "%d", p.id);
                    sprintf(remainingTime_str, "%d", p.remainigTime);
                    sprintf(sharedMem_id_cpu2_str, "%d", sharedMem_id_cpu2);
                    sprintf(CPU2_sem_id_str, "%d", CPU2_sem_id);
                    char* process_argv[] = {(char*)"./process.out",ProcessId_str,remainingTime_str,sharedMem_id_cpu2_str,CPU2_sem_id_str,NULL};
                    execv("./process.out",process_argv);
                    _exit(0);
                }
                runningPid2 = pid;
                printf("Started P%d at t=%d on CPU 2\n", runningId2, getClk());
                // file things
                FILE *fp = fopen("scheduler_2.log", "a");
                if(runningId2 != -1)
                    fprintf(fp, "At\ttime\t%d\tprocess\t%d\tstarted\n",getClk(),runningId2);
                fclose(fp);
            }
            else if(TimeToBaclFromSwitch_cpu2 == getClk()){
                switch2 = false;
                printf("Back to work normally p2\n");
            }
        }

        // CPU1
        if (cpuBusy1)
        {
            // Non-blocking check: only proceed if process.c has signalled
            int status;
            if (waitpid(runningPid1, &status, WNOHANG) > 0)
            {
                
                printf("Finished P%d at t=%d on CPU 1 | remaining=%d finished=%d\n",runningId1, getClk(),CPU1_data->remainingTime, CPU1_data->finished);
                //file things
                FILE *fp = fopen("scheduler_1.log", "a");
                fprintf(fp, "At\ttime\t%d\tprocess\t%d\tfinished\n",getClk(),runningId1);
                fclose(fp);
                //
                finishedCount++;
                cpuBusy1    = false;
                switch1 = true;
                TimeToBaclFromSwitch_cpu1 = getClk() + 1;
                runningPid1 = -1;
                runningId1  = -1;
                printf("total num of processes: %d received num = %d, finished: %d\n",totalProcesses, receivedCount, finishedCount);
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
                fprintf(fp, "At\ttime\t%d\tprocess\t%d\tfinished\n",getClk(),runningId2);
                fclose(fp);
                //
                finishedCount++;
                cpuBusy2    = false;
                switch2 = true;
                TimeToBaclFromSwitch_cpu2 = getClk() + 1;
                runningPid2 = -1;
                runningId2  = -1;
                printf("total num of processes: %d received num = %d, finished: %d\n",totalProcesses, receivedCount, finishedCount);
            }
        }

        
    }

    shmdt(CPU1_data);
    shmctl(sharedMem_id_cpu1, 0,IPC_RMID);
    semctl(CPU1_sem_id, 0, IPC_RMID);
    shmdt(CPU2_data);
    shmctl(sharedMem_id_cpu2, IPC_RMID, NULL);
    semctl(CPU2_sem_id, 0, IPC_RMID);
    free(allProcesses);
    free(cpu1IdxQ);
    free(cpu2IdxQ);
    //destroyClk(false);
    printf("%d\n",getpid());
    exit(100);
}