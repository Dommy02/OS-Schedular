#include "headers.h"

int now, syc_sem = -1;
bool isInterrupted = false;

void interrupting() {
    isInterrupted = true;
    semctl(syc_sem, 0, SETVAL, 0);
}

void continuing() {
    isInterrupted = false;
    now = getClk();
}

int main(int argc, char * argv[])
{
    signal(SIGUSR1, interrupting);
    signal(SIGCONT, continuing);
    initClk();
    now = getClk();
    int remainingTime = atoi(argv[2]);

    // shared Mem
    SharedData *data = (SharedData*) shmat(atoi(argv[3]), NULL, 0);
    
    int sem_id = atoi(argv[4]);
    if (argc == 6)
        syc_sem = atoi(argv[5]);

    printf("P%s @ time: %d has remaining time: %d\n", argv[1], now, remainingTime);

    while (remainingTime != 0)
    {
        if (isInterrupted)
            semctl(syc_sem, 0, SETVAL, 1);
        
        if (now != getClk())
        {
            now = getClk();
            remainingTime--;
            sem_wait(sem_id);
            data->remainingTime = remainingTime;
            sem_signal(sem_id);
            printf("P%s @ time: %d has remaining time: %d\n", argv[1], now, remainingTime);
            fflush(stdout);
        }
    }

    // Final synchronized state update.
    sem_wait(sem_id);
    data->remainingTime = 0;
    data->busy          = false;
    data->finished      = true;
    sem_signal(sem_id);
    printf("P%s @ time: %d finished\n", argv[1], now);
    shmdt(data);
    _exit(0);
}