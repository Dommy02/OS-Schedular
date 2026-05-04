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

void startPageTab(PT_entry* pageTable, int limit) {
    for (int i = 0; i < limit; i++) {
        pageTable[i].valid = 0;
    }
}

// this called when The process enter the ready queue for the first time to put the page table and map V_address[0] 
void putForFirstTime(Frame* ram,PT_entry* pageTable, int processID){
    int free = -1;
    for(int i = 0; i < 32; i++){
        Frame* start = &ram[i];
        if(start->taken == 0){
            free = i;
            break;
        }
    }

    if(free == -1) {
        //To DO: NRU function need to update free
        // *NRM =  should be modified if u replaced something Modified(M bit = 1) to be = 1
    }
    
    // first put the page table;
    ram[free].Is_pageTable = 1;
    ram[free].processId = processID;
    ram[free].R_M = 0;
    ram[free].taken = 1;
    // second put map v[0] to a physical place
    Frame req;
    req.Is_pageTable = 0;
    req.processId = processID;
    req.R_M = 2; // Assumption that first page has R = 1
    req.taken = 1;
    pageTable[0].phy_page = putInsideRam(ram,& req,-1, NULL);
    pageTable[0].valid = 1;
}

// helper fucntion
int putInsideRam(Frame *ram, Frame* required, int free, int* NRU_M){
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
        // *NRM =  should be modified if u replaced something Modified(M bit = 1) to be = 1
    }
    ram[free] = *required;
    return free;
}


// this function is used for any request from requests.txt
// We send the 10 bits directly to this function
int modifyData(Frame *ram,PT_entry* PT, int v_address, int R_M, int processID, int limit){
    int v_page = v_address >> 4;
    if (v_page >= limit) {
        FILE *fp = fopen("memory.log", "a");
        fprintf(fp, "Segmentation Fault !!\n");
        fclose(fp);
        return;
    }

    Frame req;
    req.R_M = R_M;
    int NRM = 0;
    if (PT[v_address].valid) {
        putInsideRam(ram, &req, PT[v_page].phy_page, &NRM);// penalty = 1
        return 1; 
    }
    else {
        /* to do: print the log

        FILE *fp = fopen("memory.log", "a");
        fprintf(fp, "Page Fault\n");
        fclose(fp);
        */

        PT[v_page].valid = 1;
        req.taken = 1;
        req.processId = processID;
        req.Is_pageTable = 0;
        PT[v_page].phy_page = putInsideRam(ram, &req, -1, &NRM);// penalty = 1 + 10
        return 11 + (NRM & 1) * 10;
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
