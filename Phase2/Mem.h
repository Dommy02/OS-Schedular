#include "headers.h"
// ram is array of pointers
typedef struct 
{
    int taken; // 1 = yes it is taken - 0 =  Not taken and u can put inside it
    int processId; // process which has it's V_page or pageTable here 
    int Is_pageTable; //
    int R_M; // 0:(0 0)  - 1:(0 1) - 2:(1 0) - 3:(1 1)
} Frame;

typedef struct 
{
    int phy_page;
    int valid;
} PT_entry;


void start_Ram(Frame *ram){
    for(int i = 0; i < 32; i++){
        ram[i].Is_pageTable = 0;
        ram[i].taken = 0;
        ram[i].R_M = 0;
        ram[i].processId = 0;
    }
}

int putInsideRam(Frame *ram, Frame* required, int free){
    if(free != -1){
        ram[free].R_M = required->R_M;
        return 0;
    }
    
    for(int i = 0; i < 32; i++){
        Frame* start = &ram[i];
        if(start->taken == 0){
            free = i;
            break;
        }
    }

    if(free == -1) {
        //To DO: NRU function need to update free
    }
    ram[free] = *required;
    return free;
}

void modifyData(Frame *ram,PT_entry* PT, int v_address, int R_M, int processID, int limit){ // We send the 6 bits directly to this function
    if (v_address >= limit) {
        FILE *fp = fopen("memory.log", "a");
        fprintf(fp, "Segmentation Fault !!\n");
        fclose(fp);
        return;
    }

    Frame req;
    req.R_M = R_M;
    if (PT[v_address].valid) {
        putInsideRam(ram, &req, PT[v_address].phy_page);
    }
    else {
        /* to do: print the log

        FILE *fp = fopen("memory.log", "a");
        fprintf(fp, "Segmentation Fault !!\n");
        fclose(fp);
        */

        PT[v_address].valid = 1;
        req.taken = 1;
        req.processId = processID;
        req.Is_pageTable = 0;
        PT[v_address].phy_page = putInsideRam(ram, &req, -1);
    }
}

void freeProcess(Frame* ram,int processId){
    for(int i = 0; i < 32; i++){
        if(ram[i].taken == 0 || ram[i].processId != processId) 
            continue;
        ram[i].Is_pageTable = 0;
        ram[i].processId = -1;
        ram[i].R_M = 0;
        ram[i].taken = 0;
    }
}

void clear_R(Frame* ram) {
    for (int i = 0; i < 32; i++) {
        ram[i].R_M &= ~2;
    }
}
