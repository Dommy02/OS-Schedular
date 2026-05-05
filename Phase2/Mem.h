#include "headers.h"
// ram is array of Frames
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

// Forward declaration
int putInsideRam(Frame *ram, Frame* required, int free, int* NRU_M);

// NRU function to find a free frame and set the Modified bit flag
int NRU(Frame *ram, int* NRU_M) {
    int target_frame = -1;
    int min_class = 4;
    
    for (int i = 0; i < 32; i++) {
        if (ram[i].taken == 1 && ram[i].Is_pageTable == 0) {
            if (ram[i].R_M < min_class) {
                min_class = ram[i].R_M;
                target_frame = i;
                if (min_class == 0) break; // Class 0 is the lowest possible, stop searching
            }
        }
    }
    
    if (target_frame != -1 && NRU_M != NULL) {
        *NRU_M = (min_class % 2); // Set to 1 if modified bit is 1 (classes 1 and 3)
    }
    
    return target_frame;
}

int NRU(Frame* ram) {
    int ram_sorted[32];
    int buffer[5] = {0, 0, 0, 0, 0};
    for (int i = 0; i < 32; i++) {
        buffer[ram[i].Is_pageTable ? 4 : ram[i].R_M]++;
    }
    for (int i = 1; i < 5; i++) {
        buffer[i] += buffer[i - 1];
    }
    int end = buffer[3];
    for (int i = 31; i >= 0; i--) {
        ram_sorted[--buffer[ram[i].Is_pageTable ? 4 : ram[i].R_M]] = i;
    }
    int stop = 0, chosen;
    while (!stop) {
        for (int i = 0; i < end; i++) {
            int index = ram_sorted[i];
            if (ram[index].R_M & 2) {
                ram[index].R_M &= 2;
            }
            else {
                chosen = index;
                stop = 1;
                break;
            }
        }
    }
    return chosen;
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

    int NRM = 0;
    if(free == -1) {
        free = NRU(ram, &NRM);
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
        // return 0;
        return free;
    }
    
    for(int i = 0; i < 32; i++){
        Frame* start = &ram[i];
        if(start->taken == 0){
            free = i;
            break;
        }
    }

    if(free == -1) {
        free = NRU(ram, NRU_M);
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
        return -1;
        // return;
    }

    Frame req;
    req.R_M = R_M;
    int NRM = 0;
    // if (PT[v_address].valid) {
    if (PT[v_page].valid) {
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
