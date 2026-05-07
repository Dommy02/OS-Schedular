#include "headers.h"
// ram is array of Frames

typedef struct 
{
    int phy_page;
    int valid;
} PT_entry;

typedef struct 
{
    int processId; // process which has it's V_page or pageTable here 
    PT_entry* pageTable; //
    int v_add;
    int pageTableIndex; // for frames aren't page table
    int R_M; // 0:(0 0)  - 1:(0 1) - 2:(1 0) - 3:(1 1)
} Frame;

typedef struct
{
    u_int32_t free; // Must be initially -1
    Frame* ramArray;
} Ram;

void start_Ram(Ram* ram) {
    ram->free = -1;
}

int NRU(Frame* ram) {
    int chosen, chosenRM = 4;
    for (int i = 0; i < 32 && chosenRM > 0; i++) {
        if (!ram[i].pageTable && ram[i].R_M < chosenRM) {
            chosen = i;
            chosenRM = ram[i].R_M;
        }
    }
    int pageTableFrame = ram[chosen].pageTableIndex;
    int modified_V_add = ram[chosen].v_add;
    ram[pageTableFrame].pageTable[modified_V_add].valid = 0;
    ram[pageTableFrame].pageTable[modified_V_add].phy_page = -1;
    return chosen;
}

// helper fucntion
int putInsideRam(Ram* ramObject, Frame* required, int free, int* NRU_RM){
    Frame* ram = ramObject->ramArray;
    if(free != -1){
        ram[free].R_M = required->R_M;
        return 0;
    }

    if(ramObject->free == 0) {
        free = NRU(ram);
        *NRU_RM = ram[free].R_M;
    }
    else {
        // Print (Free Physical page x allocated)
        free = __builtin_ctz(ramObject->free);
        *NRU_RM = 0;
    }
    ramObject->free &= ~(1 << free);
    ram[free] = *required;
    return free;
}

// this called when The process enter the ready queue for the first time to put the page table and map V_address[0] 
int putForFirstTime(Ram* ramObject, int limit, int processID){
    Frame* ram = ramObject->ramArray;
    int free;
    if(ramObject->free == 0) {
        free = NRU(ram);
    }
    else {
        free = __builtin_ctz(ramObject->free);
    }
    
    // free has the page table frame index
    // first put the page table;
    ram[free].pageTable = (PT_entry*) malloc(sizeof(PT_entry) * limit);
    for (int i = 0; i < limit; i++) {
        ram[free].pageTable[i].valid = 0;
    }
    ram[free].processId = processID;
    ram[free].R_M = 0;
    ramObject->free &= ~(1 << free);
    // second put map v[0] to a physical place
    Frame req;
    req.pageTable = NULL;
    req.processId = processID;
    req.R_M = 2; // Assumption that first page has R = 1
    req.pageTableIndex = free;
    req.v_add = 0;
    ram[free].pageTable[0].phy_page = putInsideRam(ramObject, &req, -1, NULL);// I put the physical page index
    ram[free].pageTable[0].valid = 1;

    return free; // the location of page Table inside the ram
}

// this function is used for any request from requests.txt
// We send the 10 bits directly to this function
// this function return the penaly
int modifyData(Ram* ramObject, int PT_index, int v_address, int R_M, int processID, int limit){
    Frame* ram = ramObject->ramArray;
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
        putInsideRam(ramObject, &req, ram[PT_index].pageTable[v_page].phy_page, NULL);// penalty = 1
        return v_page ? 1 : 0;
    }
    else {
        // page fault
        ram[PT_index].pageTable[v_page].valid = 1;
        req.processId = processID;
        req.pageTable = ram[PT_index].pageTable;
        ram[PT_index].pageTable[v_page].phy_page = putInsideRam(ramObject, &req, -1, &NRM);// penalty = 1 + 10
        return 11 + (NRM & 1) * 10;
    }
}

void freeProcess(Ram* ramObject, int PT_index, int limit){
    // free the frames from V_adrresses in page table
    Frame* ram = ramObject->ramArray;
    Frame* PT = ram + PT_index;
    for (int i = 0; i < limit; i++) {
        if(PT->pageTable[i].valid) {
            ramObject->free |= 1 << PT->pageTable[i].phy_page;
        }
    }
    // free the frame from page table for the process
    ramObject->free |= 1 << PT_index;
    free(ram[PT_index].pageTable);
    ram[PT_index].pageTable = NULL;
    ram[PT_index].processId  = -1;
}

void clear_R(Frame* ram) {
    for (int i = 0; i < 32; i++) {
        ram[i].R_M &= 1;
    }
}




/*
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
*/