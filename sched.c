#include "head.h"


#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

int MQ2, MQ3, SM1, SM2, other, ptp, ptf, SM;
int mtx_mmu_p, mtx_p_mmu, mtx_p_sch, mtx_m_sch;

PageTableEntry *page_tables;
int *free_frame_list, *process_totalpage, *process_totalframe;
Shared_struct *shared_resource;
proc_log *Outputs;
FILE *fp;

int main(int argc, char* argv[])
{
    struct sembuf pop = {0, -1, 0};
    struct sembuf vop = {0, 1, 0};
    // printf("Scheduling\n");

    int i, j, mtx;

    if (argc != 4)
    {
        printf("Usage: %s <Message Queue 1 ID> <Message Queue 2 ID> <# Processes>\n", argv[0]);
        exit(1);
    }
    int msgid1 = atoi(argv[1]);
    int msgid2 = atoi(argv[2]);
    int k = atoi(argv[3]);


    key_t key = ftok(".", KEY8);
    msgid1 = msgget(key, 0666);
    
    if (msgid1 == -1)
    {
        perror("msgget for MQ1 failed");
        exit(1);
    }

    key = ftok(".", KEY);
    int other = shmget(key, sizeof(Shared_struct), IPC_CREAT | 0666);
    if (other == -1)
    {
        perror("shmget for others failed");
        exit(1);
    }
    shared_resource = (Shared_struct *)shmat(other, NULL, 0);

    if(other == -1)
    {
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }


    key = ftok(".", KEY9);
    msgid2 = msgget(key, IPC_CREAT | 0666);
    if (msgid2 == -1)
    {
        perror("msgget for MQ2 failed");
        exit(1);
    }

    key = ftok(".", KEY8);
    mtx_m_sch = semget(key, 1, IPC_CREAT | 0666);
    // semctl(mtx_m_sch, 0, SETVAL, 0);


    message msg1, msg2;
    // int* term=(int*) malloc(k*sizeof(int));
    // for(int z=0;z<k;z++)
    // term[z]=0;

    while( k > 0)
    {
        // printf("Waiting on scheduler ready queue\n");
        msgrcv(msgid1, &msg1, sizeof(message), 1, 0);
        
        printf("\t\tScheduling process %d\n", msg1.value);

        // if(term[msg1.value])
        // continue;
        key_t key = ftok(".", KEY2 + msg1.value);
        mtx = semget(key, 1, IPC_CREAT | 0666);

        V(mtx);
        msgrcv(msgid2, &msg2, sizeof(message), 2, 0);
        if(msg2.value == 2)
        {
            // printf("\t\tProcess %d terminated\n",shared_resource->current_process);
            
            // if(term[shared_resource->current_process]==0)
            // {k--;}
            // term[shared_resource->current_process]=1;
            k--;
        }

        else if (msg2.value == 1)
        {
            printf("\t\tProcess %d added to end of queue\n", shared_resource->current_process);
            msg1.value = shared_resource->current_process;
            msg1.msgtype = 1;
            msgsnd(msgid1, &msg1, sizeof(message), 0);

        }
    }

    key = ftok(".",KEY10);
    SM = shmget(key, sizeof(proc_log), IPC_CREAT | 0666);
    Outputs = (proc_log *)shmat(SM, NULL, 0);
    sleep(2);
    printf("Number of Page Faults = %d\nNumber of Invalid Accesses = %d\n\n",Outputs->page_faults,Outputs->invalid_refs);
    fp = fopen("result.txt","a+");
    char temp[80];
    memset(temp,'\0',80);
    sprintf(temp,"Number of Page Faults = %d\nNumber of Invalid Accesses = %d\n\n",Outputs->page_faults,Outputs->invalid_refs);
    // fwrite(temp, 80, 1, fp);
    fprintf(fp, "Number of Page Faults = %d\nNumber of Invalid Accesses = %d\n\n",Outputs->page_faults,Outputs->invalid_refs);
    fclose(fp);


    printf("All processes terminated. Press any key to exit.\n");
    getchar();
    shmdt(Outputs);
    shmctl(SM, IPC_RMID, NULL);
    V(mtx_m_sch);

    return 0;
}