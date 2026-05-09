#ifndef MEM_H
#define MEM_H
#include <stdio.h>
#include <stdint.h>

// ram is array of Frames
typedef struct
{
    int phy_page;
    int valid;
} PT_entry;

typedef struct
{
    int processId;       // process which has it's V_page or pageTable here
    PT_entry *pageTable; //
    int v_add;           //
    int pageTableIndex;  // for frames that aren't page tables
    int R_M;             // 0:(0 0)  - 1:(0 1) - 2:(1 0) - 3:(1 1)
} Frame;

typedef struct
{
    uint32_t free; // Must be initially -1
    Frame *ramArray;
} Ram;

// done
static Ram *start_Ram()
{
    FILE *file = fopen("memory.log", "w");
    fclose(file);
    Ram *ramArray = (Ram *)malloc(sizeof(Ram));

    ramArray->free = -1;
    // free = 0b11111111111111111111111111111111 (all frames free)

    ramArray->ramArray = (Frame *)malloc(sizeof(Frame) * 32);

    // Initialize all frames
    for (int i = 0; i < 32; i++)
    {
        ramArray->ramArray[i].pageTable = NULL;
        ramArray->ramArray[i].processId = -1;
        ramArray->ramArray[i].pageTableIndex = -1;
        ramArray->ramArray[i].R_M = 0;
    }
    return ramArray;
}

// this function will be used by 2: 1- the blocked process after it returns from penalty and 2- to make it for V_address 0;
// IMP when a process leave the blocked queue to go to ready queue
// #-# when a process leave the blocked queue to go to ready queue // done
static void printLoading(Frame *ram, int time, int frameIndex_for_PT, int V_add, int base, int processId)
{ // addressOnDisk = base + V_address from the request

    int i = ram[frameIndex_for_PT].pageTable[V_add].phy_page;
    // now i is the frame number;  2 things could happen: 1-function putInFirstTime will make it for V_add[0] so I know the frame number or
    //                                                    2- the blocked function use this function so, it doesn't know the frame number
    // ******* Memory.log part *******
    FILE *file = fopen("memory.log", "a");
    fprintf(file, "At time %d disk address %d for process %d is loaded into memory page %d.\n", time, V_add + base, processId, i);
    fclose(file);
    // ******* end of Memory.log part *******
}

static int NRU(Frame *ram)
{

    int chosen, chosenRM = 4;
    for (int i = 0; i < 32 && chosenRM > 0; i++)
    {
        if (!ram[i].pageTable && ram[i].R_M < chosenRM)
        {
            chosen = i;
            chosenRM = ram[i].R_M;
        }
    }
    int pageTableFrame = ram[chosen].pageTableIndex;
    int modified_V_add = ram[chosen].v_add;
    ram[pageTableFrame].pageTable[modified_V_add].valid = 0;
    ram[pageTableFrame].pageTable[modified_V_add].phy_page = -1;
    // ******* Memory.log part *******
    FILE *file = fopen("memory.log", "a");
    fprintf(file, "Swapping out page %d to disk\n", chosen);
    fclose(file);
    // ******* end of Memory.log part *******
    return chosen;
}

// helper function
static int putInsideRam(Ram *ramObject, Frame *required, int free, int *NRU_RM)
{
    Frame *ram = ramObject->ramArray;
    if (free != -1)
    {
        ram[free].R_M = required->R_M;
        return 0;
    }

    if (ramObject->free == 0)
    {
        free = NRU(ram);
        *NRU_RM = ram[free].R_M;
    }
    else
    {
        // Print (Free Physical page x allocated)
        free = __builtin_ctz(ramObject->free);
        *NRU_RM = 0;
        // ******* Memory.log part *******
        FILE *file = fopen("memory.log", "a");
        fprintf(file, "Free Physical page %d allocated\n", free);
        fclose(file);
        // ******* end of Memory.log part *******
    }
    ramObject->free &= ~(1 << free);
    ram[free] = *required;
    return free;
}

// this called when The process enter the ready queue for the first time to put the page table and map V_address[0]

// IMP1
// ramObject pcb->limit pcb->id pcb->base currentTime
// return index of pageTable in ramObject
// #-# called at the scheduler when a process putForFirstTime // done
static int putForFirstTime(Ram *ramObject, int limit, int processID, int base, int time)
{
    Frame *ram = ramObject->ramArray;
    int free;
    if (ramObject->free == 0)
    {
        free = NRU(ram);
    }
    else
    {
        free = __builtin_ctz(ramObject->free);
    }

    // free has the page table frame index
    // first put the page table;
    ram[free].pageTable = (PT_entry *)malloc(sizeof(PT_entry) * limit);
    for (int i = 0; i < limit; i++)
    {
        ram[free].pageTable[i].valid = 0;
    }
    ram[free].processId = processID;
    ram[free].R_M = 0;
    ramObject->free &= ~(1 << free);
    // ******* Memory.log part *******
    FILE *file = fopen("memory.log", "a");
    fprintf(file, "Free Physical page %d allocated\n", free);
    fclose(file);
    // ******* end of Memory.log part *******

    // second put map v[0] to a physical place
    Frame req;
    req.pageTable = NULL;
    req.processId = processID;
    req.R_M = 2; // Assumption that first page has R = 1
    req.pageTableIndex = free;
    req.v_add = 0;
    int temp = 0;
    ram[free].pageTable[0].phy_page = putInsideRam(ramObject, &req, -1, &temp); // I put the physical page index
    ram[free].pageTable[0].valid = 1;
    // for now
    // ******* Memory.log part *******
    printLoading(ramObject->ramArray, time, free, 0, base, processID);
    // ******* end of Memory.log part *******
    return free; // the location of page Table inside the ram
}

// this function is used for any request from requests.txt
// We send the 10 bits directly to this function
// this function return the penalty

// IMP3 this is called by the scheduler when a process reads a v address
// #-# called at the scheduler when a process reads a virtual address from the file // done
static int modifyData(Ram *ramObject, int PT_index, int v_address, int R_M, int processID, int limit)
{
    Frame *ram = ramObject->ramArray;
    int v_page = v_address >> 4;
    if (v_page >= limit)
    {
        FILE *fp = fopen("memory.log", "a");
        fprintf(fp, "Segmentation Fault !!\n");
        fclose(fp);
        return -1;
        // return;
    }
    Frame req;
    req.R_M = R_M;
    int NRM = 0;

    if (ram[PT_index].pageTable[v_page].valid)
    {
        putInsideRam(ramObject, &req, ram[PT_index].pageTable[v_page].phy_page, NULL); // penalty = 1
        return v_page ? 1 : 0;
    }
    else
    {
        // page fault
        // ******* Memory.log part *******
        if (v_address != -1)
        {
            FILE *file = fopen("memory.log", "a");
            fprintf(file, "PageFault upon VA 0x%x from process %d\n", v_address, processID);
            fclose(file);
        }
        // ******* end of Memory.log part *******
        ram[PT_index].pageTable[v_page].valid = 1;
        req.processId = processID;
        req.pageTable = ram[PT_index].pageTable;
        ram[PT_index].pageTable[v_page].phy_page = putInsideRam(ramObject, &req, -1, &NRM); // penalty = 1 + 10
        return 11 + (NRM & 1) * 10;
    }
}

// #-# called when a process is finished  // done
static void freeProcess(Ram *ramObject, int PT_index, int limit)
{
    // free the frames from V_addresses in page table
    Frame *ram = ramObject->ramArray;
    Frame *PT = ram + PT_index;
    for (int i = 0; i < limit; i++)
    {
        if (PT->pageTable[i].valid)
        {
            ramObject->free |= 1 << PT->pageTable[i].phy_page;
        }
    }
    // free the frame from page table for the process
    ramObject->free |= 1 << PT_index;
    free(ram[PT_index].pageTable);
    ram[PT_index].pageTable = NULL;
    ram[PT_index].processId = -1;
}

// #-# called every k quantum's // done
static void clear_R(Frame *ram)
{
    for (int i = 0; i < 32; i++)
    {
        ram[i].R_M &= 1;
    }
}

#endif