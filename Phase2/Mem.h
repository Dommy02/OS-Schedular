typedef struct Frame
{
    int taken; // 1 = yes it is taken - 0 =  Not taken and u can put inside it
    int processId; // process which has it's V_page or pageTable here 
    int Is_pageTable;
    int v_page;
    int R,M;
} Frame;

typedef struct PT_entry
{
    int phy_page;
    int valid;
} PT_entry;


void start_Ram(Frame* ram){
    int counter = 32;
    while(counter--){
        ram->Is_pageTable = 0;
        ram->taken = 0;
        ram->M = 0;
        ram->R = 0;
        ram->processId = 0;
        ram->v_page = -1;
    }
}

int getFirstFree(Frame* ram){
    int free = -1;
    Frame * start = ram;
    int counter = 0;
    while(counter != 32 && free == -1){
        if(start->taken == 0){
            free = counter;
        }
        else{
            counter++;
            start = start+counter;
        }
    }

    return free;
}

int putInsideRam(Frame * ram){ // will put inside the ram
    int freeFrame = getFirstFree(ram);
    
}