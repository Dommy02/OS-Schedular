#include "headers.h"
#include "utils.h"

/* Modify this file as needed*/

int sharedMemID;
PCB *sharedMemPCBPtr;

int semProcessTurn;
int semSchedulerTurn;

int main(int argc, char *argv[])
{
    /*
        argv[0] = "process.c"
        argv[1] = sharedMemID
        argv[2] = semProcessTurn;
        argv[3] = semSchedulerTurn;
    */
    // printf("|##########################|-> Entered Child\n");
    // raise(SIGSTOP); // waiting for shared memory initialization

    initClk();

    sharedMemID = atoi(argv[1]);
    semProcessTurn = atoi(argv[2]);
    semSchedulerTurn = atoi(argv[3]);
    sharedMemPCBPtr = (PCB *)shmat(sharedMemID, NULL, 0);

    // printf("|##########################|-> IN Child ID : %d, Time : %d\n", sharedMemPCBPtr->id, getClk());

    int lastTickTime = -1;

    while (sharedMemPCBPtr->remaining_time > 0)
    {
        // should down the is child done
        int currentTime = getClk();

        if (lastTickTime < currentTime)
        {
            //printf("In child the semProcessTurn Before : %d\n", semProcessTurn);
            down(semProcessTurn);
            //printf("In child the semProcessTurn After : %d\n", semProcessTurn);
            currentTime = getClk();
        }

        int elapsedTime = currentTime - sharedMemPCBPtr->relative_time;
        if (elapsedTime > 0)
        {
            // printf("ID : %d, Remaining Time : %d, Elapsed Time : %d, Current Time : %d\n", sharedMemPCBPtr->id, sharedMemPCBPtr->remaining_time, elapsed, currentTime);
            sharedMemPCBPtr->remaining_time -= elapsedTime; // (SHOULD UPDATE) (NOT DONE) : check that this should always be 1 or can be greater than that
            sharedMemPCBPtr->relative_time = currentTime;   // check here as well if it's only a +1
        }

        if (sharedMemPCBPtr->remaining_time == 0)
        {
            break;
            // (SHOULD UPDATE) (NOT DONE) : the goal here is to up in the SIGCHLD section
            // (OLD COMMENT) if this is the last run i will release the semaphore at the delete in SIGCHLD
        }

        if (lastTickTime < currentTime)
        {
            // printf("In child the semSchedulerTurn Before : %d\n", semSchedulerTurn);
            up(semSchedulerTurn);
            // printf("In child the semSchedulerTurn After : %d\n", semSchedulerTurn);
        }

        lastTickTime = currentTime;
    }

    destroyClk(false);
    shmdt(sharedMemPCBPtr);

    return 0;
}