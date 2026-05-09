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
    int wasInterrupted = 0;
    int timeInCpu = 0;

    ///////////////////////////////////////////////////////////////////////////////
    // file reading
    char filename[50];
    sprintf(filename, "requests_%d.txt", sharedMemPCBPtr->id);
    FILE *requestsFile = fopen(filename, "r");
    if (requestsFile == NULL)
    {
        printf("Error in Opening %s File", filename);
        kill(getppid(), SIGINT);
        exit(-1);
    }
    char line[256];
    fgets(line, sizeof(line), requestsFile); // the commented line '#'
    long long currentPosition = ftell(requestsFile);

    while (sharedMemPCBPtr->remaining_time > 0)
    {
        // should down the is child done
        int currentTime = getClk();

        if (lastTickTime < currentTime)
        {
            // printf("In child the semProcessTurn Before : %d\n", semProcessTurn);
            down(semProcessTurn);
            // printf("In child the semProcessTurn After : %d\n", semProcessTurn);
            currentTime = getClk();
            wasInterrupted = 0;
        }

        int elapsedTime = currentTime - sharedMemPCBPtr->relative_time;
        if (elapsedTime > 0)
        {
            timeInCpu++;
            // printf("ID : %d, Remaining Time : %d, Elapsed Time : %d, Current Time : %d\n", sharedMemPCBPtr->id, sharedMemPCBPtr->remaining_time, elapsed, currentTime);
            sharedMemPCBPtr->remaining_time -= elapsedTime; // (SHOULD UPDATE) (NOT DONE) : check that this should always be 1 or can be greater than that
            sharedMemPCBPtr->relative_time = currentTime;   // check here as well if it's only a +1

            // TODO read from requests file and kill siguser2 to parent after populating the shared mem ptr
            if (fgets(line, sizeof(line), requestsFile) != NULL)
            {
                int readTime, vAddressHex;
                char readState;
                sscanf(line, "%d %x %c", &readTime, &vAddressHex, &readState);
                if (timeInCpu >= readTime)
                {
                    currentPosition = ftell(requestsFile);
                    // let the fgets advance the pointer
                    sharedMemPCBPtr->last_request_hex = vAddressHex;
                    sharedMemPCBPtr->last_request_state = readState;
                    kill(getppid(), SIGUSR2);
                    raise(SIGSTOP);
                }
                else
                {
                    fseek(requestsFile, currentPosition, SEEK_SET);
                }
            }
        }

        if (sharedMemPCBPtr->remaining_time == 0)
        {
            break;
            // (DONE) : the goal here is to up in the SIGCHLD section
            // (OLD COMMENT) (DONE) if this is the last run i will release the semaphore at the delete in SIGCHLD
        }

        if (lastTickTime < currentTime && wasInterrupted == 0)
        {
            // printf("In child the semSchedulerTurn Before : %d\n", semSchedulerTurn);
            if (sharedMemPCBPtr->p_isInterrupted)
            {
                sharedMemPCBPtr->p_isInterrupted = 0;
            }
            else
            {
                up(semSchedulerTurn);
            }
            // printf("In child the semSchedulerTurn After : %d\n", semSchedulerTurn);
        }

        lastTickTime = currentTime;
    }

    fclose(requestsFile);
    destroyClk(false);
    shmdt(sharedMemPCBPtr);

    return 0;
}