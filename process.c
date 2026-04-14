#include "headers.h"



int main(int argc, char * argv[])
{
    initClk();
    int now           = getClk();
    int remainingTime = atoi(argv[2]);

    // shared Mem
    SharedData *data = (SharedData*) shmat(atoi(argv[3]), NULL, 0);
    if (data == (void*)-1) { perror("shmat"); _exit(1); }

    int sem_id = atoi(argv[4]);

    printf("P%s @ time: %d has remaining time: %d\n", argv[1], now, remainingTime);

    while (remainingTime != 0)
    {
        if (now != getClk())
        {
            now = getClk();
            remainingTime--;
            data->remainingTime = remainingTime;
            printf("P%s @ time: %d has remaining time: %d\n", argv[1], now, remainingTime);
        }
    }

    // write final state first, THEN signal
    data->remainingTime = 0;
    data->busy          = false;
    data->finished      = true;
    
    sem_signal(sem_id);
    printf("P%s @ time: %d finished\n", argv[1], now);
    shmdt(data);
    _exit(0);
}