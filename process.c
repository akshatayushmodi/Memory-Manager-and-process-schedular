#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <wait.h>
#include <sys/time.h>
#include <sys/sem.h>
#include <signal.h>
#include "head.h"

#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

int MQ2, MQ3, SM1, SM2, other, ptp, ptf;
int mtx_mmu_p, mtx_p_mmu, mtx_p_sch;

PageTableEntry *page_tables;
int *free_frame_list, *process_totalpage, *process_totalframe;
Shared_struct *shared_resource;


int main(int argc, char* argv[])
{
    key_t key = ftok(".", KEY);
    int other = shmget(key, sizeof(Shared_struct), IPC_CREAT | 0666);
    if (other == -1)
    {
        perror("shmget for others failed");
        exit(1);
    }
    shared_resource = (Shared_struct *)shmat(other, NULL, 0);
    struct sembuf pop = {0, -1, 0};
    struct sembuf vop = {0, 1, 0};

    int i, j;
    if(argc <  6)
    {
        printf("Usage: %s <Reference String> <Virtual Address Space size> <Physical Address Space size> <pid> <reference string length> \n", argv[0]);
        exit(1);
    }
    int msgid1 = atoi(argv[2]);
    int msgid3 = atoi(argv[3]);
    int pidx = atoi (argv[4]);
    int reflen = atoi(argv[5]);
    char reference[reflen+1];
    strcpy(reference, argv[1]);
    reference[reflen]='\0';
    printf("Reference string of proc %d is\n %s\n",pidx, reference);

    
    
    key = ftok(".", KEY3);
    mtx_mmu_p = semget(key, 1, 0666);
    
    key = ftok(".", KEY4);
    mtx_p_mmu = semget(key, 1, 0666);

    key = ftok(".", KEY2+pidx);
    int mtx_pi = semget(key, 1, 0666);

    key = ftok(".", KEY5);
    mtx_p_sch = semget(key, 1, IPC_CREAT | 0666);

    message msg1;
    msg1.value = pidx;
    msg1.msgtype = 1;
    
    
    key = ftok(".", KEY8);
    msgid1 = msgget(key, 0666);
    
    if (msgid1 == -1)
    {
        perror("msgget for MQ1 failed");
        exit(1);
    }

    key = ftok(".", KEY10);
    msgid3 = msgget(key, 0666);
    
    if (msgid3 == -1)
    {
        perror("msgget for MQ1 failed");
        exit(1);
    }

    if(msgsnd(msgid1, &msg1, sizeof(message), 0) < 0){
        perror("ready queue send failed1");
        exit(EXIT_FAILURE);
}   
// printf("Waiting to be scheduled\n");
    P(mtx_pi);
    shared_resource->current_process = pidx;
    for(i = 0; i < reflen; ++i)
    {
    // printf("Scheduled successfully\n");
        j = 0;
        while( i < reflen && reference[i] != '.')
        {
            j = j*10 + (reference[i] - '0');
            ++i;
            // printf("Inf\n");
        }
        // printf("Page number is %d\n",j);
        message msg;
        msg.value = j;
        msg.msgtype =2;
        if(msgsnd(msgid3, &msg, sizeof(message), 0) < 0){
        perror("ready queue send failed");
        exit(EXIT_FAILURE);
}       
        // printf("Process signals mmu mmu_p %u\n",mtx_mmu_p);
        V(mtx_mmu_p);
        // printf("Waiting on mmu p_mmu %u\n",mtx_p_mmu);
        P(mtx_p_mmu);
        // printf("mmu triggered me\n");
        msgrcv(msgid3, &msg, sizeof(message), 2, 0);
        j = msg.value;
        if( j == -2 )
        {
            // printf("Process %d: Illegal Page Number\nTerminating\n",pidx);
            exit(EXIT_FAILURE);
        }
        else if(j == -1)
        {
            // printf("Process %d: Page Fault, Waiting...\n",pidx);
            P(mtx_pi);
            shared_resource->current_process = pidx;
            continue;
        }
        else
        {
            // printf("Process %d: ", pidx);
            // printf("Frame %d allocated\n", msg.value);
            shared_resource->current_process = pidx;
            // V(mtx_p_mmu);
        }
    }
    // send the termination signal to the mmu
    // FILE* fp = fopen("result.txt","a+");
    // char temp[50];
    // memset(temp,'\0',50);
    // sprintf(temp,"Process %d: \nGot all frames properly\n",pidx);
    // // fwrite(temp, 50, 1, fp);
    // // fprintf(fp, "Process %d: \nGot all frames properly\n",pidx);
    // fclose(fp);
    msg1.value = -9;
    msg1.msgtype = 2;
    msgsnd(msgid3, &msg1, sizeof(message), 0);
    V(mtx_mmu_p);
}