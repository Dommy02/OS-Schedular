#include "headers.h"
// ram is array of Frames

typedef struct 
{
    int phy_page;
    int valid;
} PT_entry;

typedef struct 
{
    int taken; // 1 = yes it is taken - 0 =  Not taken and u can put inside it
    int processId; // process which has it's V_page or pageTable here 
    PT_entry* pageTable; //
    int v_add;
    int pageTableIndex; // for frames aren't page table
    int R_M; // 0:(0 0)  - 1:(0 1) - 2:(1 0) - 3:(1 1)
} Frame;

void start_Ram(Frame *ram) {
    for(int i = 0; i < 32; i++){
        ram[i].pageTable = NULL;
        ram[i].taken = 0;
        ram[i].R_M = 0;
        ram[i].processId = -1;
        ram[i].v_add = -1;
        ram[i].pageTableIndex = -1;
    }
}

int NRU(Frame* ram) {
    int ram_sorted[32];
    int buffer[5] = {0, 0, 0, 0, 0};
    for (int i = 0; i < 32; i++) {
        buffer[ram[i].pageTable ? 4 : ram[i].R_M]++;
    }
    for (int i = 1; i < 5; i++) {
        buffer[i] += buffer[i - 1];
    }
    int end = buffer[3]; // because, buffer[4] for page tables
    for (int i = 31; i >= 0; i--) {
        ram_sorted[--buffer[ram[i].pageTable ? 4 : ram[i].R_M]] = i;
    }

    int chosen = -1;
    for (int i = 0; i < end; i++) {
        int index = ram_sorted[i];
        if (ram[index].R_M & 2) {
            ram[index].R_M &= 1; // from refrenced to not refrenced
            // 11 -> 01 (3 -> 1) , (10 -> 00)(2 -> 0)
            // {2, 2, 2}
        }
        else {
            chosen = index;
            break;
        }
    }

    if (chosen == -1) {
        chosen = ram_sorted[0];
    }
    
    int pageTableFrame = ram[chosen].pageTableIndex;
    int modified_V_add = ram[chosen].v_add;
    ram[pageTableFrame].pageTable[modified_V_add].valid = 0;
    ram[pageTableFrame].pageTable[modified_V_add].phy_page = -1;
    return chosen;
}

// this called when The process enter the ready queue for the first time to put the page table and map V_address[0] 
int putForFirstTime(Frame* ram, int limit, int processID){
    int free = -1;
    for(int i = 0; i < 32; i++){
        Frame* start = &ram[i];
        if(start->taken == 0){
            free = i;
            break;
        }
    }

    
    if(free == -1) {
        free = NRU(ram);
    }
    
    // free has the page table frame index
    // first put the page table;
    ram[free].pageTable = (PT_entry*) malloc(sizeof(PT_entry) * limit);
    for (int i = 0; i < limit; i++) {
        ram[free].pageTable[i].valid = 0;
    }
    ram[free].processId = processID;
    ram[free].R_M = 0;
    ram[free].taken = 1;

    // second put map v[0] to a physical place
    Frame req;
    req.pageTable = NULL;
    req.processId = processID;
    req.R_M = 2; // Assumption that first page has R = 1
    req.taken = 1;
    req.pageTableIndex = free;
    req.v_add = 0;
    ram[free].pageTable[0].phy_page = putInsideRam(ram, &req,-1, NULL);// I put the physical page index
    ram[free].pageTable[0].valid = 1;

    return free; // the location of page Table inside the ram
}

// helper fucntion
int putInsideRam(Frame *ram, Frame* required, int free, int* NRU_RM){
    if(free != -1){
        ram[free].R_M = required->R_M;
        // return 0;
        return free;
    }
    
    for(int i = 0; i < 32; i++){
        if(ram[i].taken == 0){
            free = i;
            break;
        }
    }

    if(free == -1) {
        free = NRU(ram);
        *NRU_RM = ram[free].R_M;
    }
    else {
        // Print (Free Physical page x allocated)
        *NRU_RM = 0;
    }
    ram[free] = *required;
    return free;
}


// this function is used for any request from requests.txt
// We send the 10 bits directly to this function
// this function return the penaly
int modifyData(Frame *ram, int PT_index, int v_address, int R_M, int processID, int limit){
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
    if (ram[PT_index].pageTable[v_page].valid) {
        putInsideRam(ram, &req, ram[PT_index].pageTable[v_page].phy_page, NULL);// penalty = 1
        return v_page ? 1 : 0;
    }
    else {
        // page fault
        ram[PT_index].pageTable[v_page].valid = 1;
        req.taken = 1;
        req.processId = processID;
        req.pageTable = ram[PT_index].pageTable;
        ram[PT_index].pageTable[v_page].phy_page = putInsideRam(ram, &req, -1, &NRM);// penalty = 1 + 10
        return 11 + (NRM & 1) * 10;
    }
}

void freeProcess(Frame* ram,int PT_index, int limit){
    // free the frames from V_adrresses in page table
    Frame* PT = ram + PT_index;
    for (int i = 0; i < limit; i++) {
        if(PT->pageTable[i].valid) {
            
            ram[PT->pageTable[i].phy_page].taken = 0;
        }
    }
    // free the frame from page table for the process
    ram[PT_index].taken = 0;
    free(ram[PT_index].pageTable);
    ram[PT_index].pageTable = NULL;
    ram[PT_index].processId  = -1;
}

void clear_R(Frame* ram) {
    for (int i = 0; i < 32; i++) {
        ram[i].R_M &= ~2;
    }
}




/*
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
*/