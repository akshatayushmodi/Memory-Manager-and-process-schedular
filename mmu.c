#include "head.h"

int MQ2, MQ3, SM1, SM2, other, ptp, ptf, SM;
int mtx_mmu_p, mtx_p_mmu;
FILE* fp;

// Global Variables
PageTableEntry *page_tables;
int *free_frame_list, *process_totalpage, *process_totalframe;
Shared_struct *shared_resource;
proc_log *Outputs;

int k, f, m;

int time_stamp = 0;

void wait_semaphore(int semid)
{
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = -1;
    sb.sem_flg = 0;
    semop(semid, &sb, 1);
}

void signal_semaphore(int semid)
{
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = 1;
    sb.sem_flg = 0;
    semop(semid, &sb, 1);
}

// Function Prototypes
void retrieve_frame(int process_id, int page_number);
void handle_page_fault(int process_id, int page_number);

int main(int argc, char *argv[])
{

    if (argc != 5)
    {
        printf("Usage: %s <MQ2> <MQ3> <SM1> <SM2>\n", argv[0]);
        exit(1);
    }
    fp = fopen("result.txt","w");
    fclose(fp);
    MQ2 = atoi(argv[1]);
    MQ3 = atoi(argv[2]);
    SM1 = atoi(argv[3]);
    SM2 = atoi(argv[4]);
    

    key_t key = ftok(".",KEY10);
    SM = shmget(key, sizeof(proc_log), IPC_CREAT | 0666);
    Outputs = (proc_log *)shmat(SM, NULL, 0);

    Outputs->invalid_refs = 0;
    Outputs->page_faults = 0;

    
    // Attach to shared memory for page tables
    page_tables = (PageTableEntry *)shmat(SM1, NULL, 0);

    // Attach to shared memory for free frame list
    free_frame_list = (int *)shmat(SM2, NULL, 0);

    // Attach other resources
    key = ftok(".", KEY);
    other = shmget(key, sizeof(Shared_struct), IPC_CREAT | 0666); 
    if (other == -1)
    {
        perror("shmget for others failed");
        exit(1);
    }
    shared_resource = (Shared_struct *)shmat(other, NULL, 0);

    // Create shared memory total page for a process
    key = ftok(".", KEY6);
    ptp = shmget(key, k * sizeof(int), IPC_CREAT | 0666);
    if (ptp == -1)
    {
        perror("shmget shared memory total page for a process failed");
        exit(1);
    }
    process_totalpage = (int *)shmat(ptp, NULL, 0);

    // Create shared memory total frame for a process
    key = ftok(".", KEY7);
    ptf = shmget(key, k * sizeof(int), IPC_CREAT | 0666);
    if (ptf == -1)
    {
        perror("shmget shared memory total frame for a process failed");
        exit(1);
    }
    process_totalframe = (int *)shmat(ptf, NULL, 0);


    f = shared_resource->f;
    m = shared_resource->m;
    k = shared_resource->k;

    key = ftok(".", KEY3);
    mtx_mmu_p = semget(key, 1, IPC_CREAT | 0666);

    key = ftok(".", KEY4);
    mtx_p_mmu = semget(key, 1, IPC_CREAT | 0666);

    int total_processes_completed = 0;

    while (1)
    {
        // Receive page number from process (Message Queue 3)
        int process_id;
        message page_number;

        // printf("Waiting on process signal mmu_p%u\n",mtx_mmu_p);
        wait_semaphore(mtx_mmu_p);
        // printf("process signaled\n");
        if (msgrcv(MQ3, &page_number, sizeof(message), 2, 0) == -1)
        {
            perror("msgrcv failed");
            exit(1);
        }
        process_id = shared_resource->current_process;

        time_stamp++;
        printf("Timestamp %d      %d      %d\n", time_stamp, process_id, page_number.value); 


        if(page_number.value == -9)
        {
            for (int j = 0; j < process_totalpage[process_id]; j++)
            {
                if(page_tables[process_id * m + j].valid_bit == 1)
                    free_frame_list[page_tables[process_id * m + j].frame_number] = -1;
            }
            

            // Send frame number to scheduler (Message Queue 2)
            message type_two = {2, 2};
            if (msgsnd(MQ2, &type_two, sizeof(message), 0) == -1)
            {
                perror("msgsnd failed");
                exit(1);
            }
            total_processes_completed++;

            continue;
        }

        if(page_number.value < 0 || page_number.value >= process_totalpage[process_id])
        {
            message frame_number = {2,-2};
            // Send -2 to process (Message Queue 3)
            if (msgsnd(MQ3, &frame_number, sizeof(message), 0) == -1)
            {
                perror("msgsnd failed");
                exit(1);
            }
            Outputs->invalid_refs++;
            signal_semaphore(mtx_p_mmu);

            printf("INVALID PAGE REFERENCE :     %d    %d\n", process_id, page_number.value);

            fp = fopen("result.txt","a+");
            char temp[50];
            memset(temp,'\0',50);
            sprintf(temp, "INVALID PAGE REFERENCE :     %d    %d\n", process_id, page_number.value);
            // fwrite(temp, 50, 1, fp);
            fprintf(fp,  "INVALID PAGE REFERENCE :     %d    %d\n", process_id, page_number.value);
            fclose(fp);
            // Send frame number to scheduler (Message Queue 2)
            message type_two = {2, 2};
            if (msgsnd(MQ2, &type_two, sizeof(message), 0) == -1)
            {
                perror("msgsnd failed");
                exit(1);
            }
            total_processes_completed++;

            continue;
        }

        // Check page table
        if (page_tables[process_id * m + page_number.value].valid_bit == 1)
        {
            
            retrieve_frame(process_id, page_number.value);
        }
        else
        {
            Outputs->page_faults++;
            printf("PAGE FAULT SEQUENCE :   %d    %d\n", process_id, page_number.value);
            fp = fopen("result.txt","a+");
            char temp[50];
            memset(temp,'\0',50);
            sprintf(temp,"PAGE FAULT SEQUENCE :   %d    %d\n", process_id, page_number.value);
            // fwrite(temp, 50, 1, fp);
            fprintf(fp ,"PAGE FAULT SEQUENCE :   %d    %d\n", process_id, page_number.value);
            fclose(fp);
            handle_page_fault(process_id, page_number.value);
        }
    }

    return 0;
}

void retrieve_frame(int process_id, int page_number)
{
    message frame_number = {2,page_tables[process_id * m + page_number].frame_number};
    printf("PAGE HIT :   %d    %d    %d\n", process_id, page_number, frame_number.value);
    int lr_time=free_frame_list[page_tables[process_id * m + page_number].frame_number];
    free_frame_list[page_tables[process_id * m + page_number].frame_number]=0;
    for (int i = 0; i < process_totalpage[process_id]; i++)
        {
            if(page_tables[process_id * m + i].valid_bit == 1)
            {
                if(free_frame_list[page_tables[process_id * m + i].frame_number] <= lr_time)
                {
                    // lru_page = i;
                 free_frame_list[page_tables[process_id * m + i].frame_number]++;
                }
            }
        }
    // Send frame number to process (Message Queue 3)
    if (msgsnd(MQ3, &frame_number, sizeof(message), 0) == -1)
    {
        perror("msgsnd failed");
        exit(1);
    }
    signal_semaphore(mtx_p_mmu);
}

void handle_page_fault(int process_id, int page_number)
{
    message neg_one = {2, -1};
    if (msgsnd(MQ3, &neg_one, sizeof(message), 0) == -1)
    {
        perror("msgsnd failed");
        exit(1);
    }
    signal_semaphore(mtx_p_mmu);

    int frame_number = -1; // Placeholder for frame number
    int i;

    // Check if it has taken max frames it can get
    int flag_maxframetaken = 0, taken = 0;
    for (int i = 0; i < process_totalpage[process_id]; i++)
    {
        if(page_tables[process_id * m + i].valid_bit == 1)
        {
            taken++;
        }
    }
    if(taken == process_totalframe[process_id])
    {
        flag_maxframetaken = 1;
        // printf("MAX FRAME TAKEN : %d\n", process_id);
    }

    // Check for free frames
    for (i = 0; i < f && (!flag_maxframetaken); i++)
    {
        if (free_frame_list[i] == -1)
        {
            frame_number = i;
            free_frame_list[i] = 0; // Mark frame as occupied
            for (i = 0; i < process_totalpage[process_id]; i++)
                {
                    if(page_tables[process_id * m + i].valid_bit == 1)
                    {
                        if(page_tables[process_id * m + i].frame_number!=frame_number)
                        {
                            // lru_page = i;
                            free_frame_list[page_tables[process_id * m + i].frame_number]++;
                        }
                    }
                }
            break;
        }
    }

    // If no free frames, use LRU replacement
    if (frame_number == -1)
    {
        // TODO: Implement LRU logic
        int lru_page, lr_time = -1;
        int z;
        for (i = 0; i < process_totalpage[process_id]; i++)
        {
            if(page_tables[process_id * m + i].valid_bit == 1)
            {
                if(free_frame_list[page_tables[process_id * m + i].frame_number] > lr_time)
                {
                    lru_page = i;
                    lr_time = free_frame_list[page_tables[process_id * m + i].frame_number];
                }
            }
        }
        if(lr_time!=-1){
        page_tables[process_id * m + lru_page].valid_bit = 0;
        frame_number = page_tables[process_id * m + lru_page].frame_number;
        free_frame_list[frame_number] = 0;
        for (i = 0; i < process_totalpage[process_id]; i++)
        {
            if(page_tables[process_id * m + i].valid_bit == 1)
            {
                if(free_frame_list[page_tables[process_id * m + i].frame_number] <= lr_time)
                {
                    // lru_page = i;
                 free_frame_list[page_tables[process_id * m + i].frame_number]++;
                }
            }
        }
        }

        if(frame_number==-1){
        for(z=0;z<k;z++){
        for (i = 0; i < process_totalpage[z]; i++)
        {
            if(page_tables[z * m + i].valid_bit == 1)
            {
                // printf("validbit\n");
                if(free_frame_list[page_tables[z * m + i].frame_number] > lr_time)
                {
                    lru_page = i;
                    lr_time = free_frame_list[page_tables[z * m + i].frame_number];
                }
            }
        }
        }
        page_tables[z * m + lru_page].valid_bit = 0;
        // printf("lru page : %d %lld\n", lr_time, GetTimeStamp());
        free_frame_list[frame_number] = 0;
        frame_number = page_tables[z * m + lru_page].frame_number;
        for (i = 0; i < process_totalpage[z]; i++)
        {
            if(page_tables[z * m + i].valid_bit == 1)
            {
                if(free_frame_list[page_tables[z * m + i].frame_number] <= lr_time)
                {
                    // lru_page = i;
                 free_frame_list[page_tables[z * m + i].frame_number]++;
                }
            }
        }
        }
    }

    // Update page table
    page_tables[process_id * m + page_number].frame_number = frame_number;
    page_tables[process_id * m + page_number].valid_bit = 1;

    // Send frame number to scheduler (Message Queue 2)
    message type_one = {2, 1};
    if (msgsnd(MQ2, &type_one, sizeof(message), 0) == -1)
    {
        perror("msgsnd failed");
        exit(1);
    }
}

