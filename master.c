#include "head.h"

int SM1, SM2, MQ1, MQ2, MQ3, other, ptp, ptf, SM;
int *mtx, mtx_mmu_p, mtx_p_mmu, mtx_p_sch, mtx_m_sch;
int k, m, f;

// Global Variables
PageTableEntry *page_tables;
int *free_frame_list, *process_totalpage, *process_totalframe;
Shared_struct *shared_resource;

int mmu_pid, sched_id;


int findCount(unsigned int n)
{
    unsigned int count = 0;
    unsigned int temp = 1;
    while (temp <= n) {
        count++;
        temp *= 10;
    }
    return count;
}

int main()
{
    // Read inputs
    printf("Enter the total number of processes (k): ");
    scanf("%d", &k);
    printf("Enter the maximum number of pages required per process (m): ");
    scanf("%d", &m);
    printf("Enter the total number of frames (f): ");
    scanf("%d", &f);

    // Create shared memory and message queues
    
    key_t key = ftok(".", KEY);
    other = shmget(key, sizeof(Shared_struct), IPC_CREAT | 0666);
    if (other == -1)
    {
        perror("shmget for others failed");
        exit(1);
    }
    shared_resource = (Shared_struct *)shmat(other, NULL, 0);
    shared_resource->f = f;
    shared_resource->m = m;
    shared_resource->k = k;
    shared_resource->current_process = -1;

    // Create shared memory for page tables
    SM1 = shmget(IPC_PRIVATE, (k * m * sizeof(PageTableEntry)), IPC_CREAT | 0666);
    if (SM1 == -1)
    {
        perror("shmget for page tables failed");
        exit(1);
    }
    page_tables = (PageTableEntry *)shmat(SM1, NULL, 0);

    // Create shared memory for free frame list
    SM2 = shmget(IPC_PRIVATE, f * sizeof(int), IPC_CREAT | 0666);
    if (SM2 == -1)
    {
        perror("shmget for free frame list failed");
        exit(1);
    }
    free_frame_list = (int *)shmat(SM2, NULL, 0);

    //check
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
    //uncheck
    
    mtx = (int *)malloc(k * sizeof(int));
    for (int i = 0; i < k; i++)
    {
        key_t key = ftok(".", KEY2 + i);
        mtx[i] = semget(key, 1, IPC_CREAT | 0666);
        semctl(mtx[i], 0, SETVAL, 0);
    }

    key = ftok(".", KEY3);
    mtx_mmu_p = semget(key, 1, IPC_CREAT | 0666);
    semctl(mtx_mmu_p, 0, SETVAL, 0);
    
    key = ftok(".", KEY4);
    mtx_p_mmu = semget(key, 1, IPC_CREAT | 0666);
    semctl(mtx_p_mmu, 0, SETVAL, 0);
    
    key = ftok(".", KEY5);
    mtx_p_sch = semget(key, 1, IPC_CREAT | 0666);
    semctl(mtx_p_sch, 0, SETVAL, 1);

    key = ftok(".", KEY8);
    mtx_m_sch = semget(key, 1, IPC_CREAT | 0666);
    semctl(mtx_m_sch, 0, SETVAL, 0);


    key = ftok(".", KEY8);
    MQ1 = msgget(key, IPC_CREAT | 0666);
    if (MQ1 == -1)
    {
        perror("msgget for MQ1 failed");
        exit(1);
    }
    // printf("MQ1 : %d\n", MQ1);

    // Create message queue for communication between scheduler and MMU
    key = ftok(".", KEY9);
    MQ2 = msgget(key, IPC_CREAT | 0666);
    if (MQ2 == -1)
    {
        perror("msgget for MQ2 failed");
        exit(1);
    }
    key = ftok(".", KEY10);
    // Create message queue for communication between process and MMU
    MQ3 = msgget(key, IPC_CREAT | 0666);
    if (MQ3 == -1)
    {
        perror("msgget for MQ3 failed");
        exit(1);
    }

    // Initialize data structures
    for (int i = 0; i < k * m; i++)
    {
        page_tables[i].valid_bit = 0;
    }
    for (int i = 0; i < f; i++)
    {
        free_frame_list[i] = -1;
    }

    usleep(10000);
    // Create scheduler and MMU processes
    
    pid_t sched_id = fork();
    if (sched_id == 0)
    {
        // Child process
        char arg1[10], arg2[10], arg3[10];
        sprintf(arg1, "%d", MQ1);
        sprintf(arg2, "%d", MQ2);
        sprintf(arg3, "%d", k);
        execl("./sched", "./sched", arg1, arg2, arg3, NULL);
        exit(0);
    }
    else if (sched_id < 0)
    {
        perror("fork failed");
        exit(1);
    }

    
    pid_t mmu_pid = fork();
    if (mmu_pid == 0)
    {
        // Child process
        char arg1[10], arg2[10], arg3[10], arg4[10];
        sprintf(arg1, "%d", MQ2);
        sprintf(arg2, "%d", MQ3);
        sprintf(arg3, "%d", SM1);
        sprintf(arg4, "%d", SM2);
        //"xterm", "xterm", "-T", "Memory Management Unit", "-e",
        execlp( "xterm", "xterm", "-T", "Memory Management Unit", "-e", "./mmu", arg1, arg2, arg3, arg4, NULL);
        exit(0);
    }
    else if (mmu_pid < 0)
    {
        perror("fork failed");
        exit(1);
    }

    
    int r1, r2;
    for (int i = 0; i < k; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            // Child process
            r1 = rand() % m+1;
            r2 = rand() % (8*r1) + 2*r1;
            char arg1[4*r2];
            int k = 0, temp, cnt = 0;
            for(int j = 0 ; j < 4*r2; ++j)
            {
                arg1[j] = '.';
            }
            int jfk;
            for (jfk = 0; jfk < 4*r2; ++jfk)
            {
                temp = rand() % r1+1;
                cnt = findCount(temp);
                sprintf(arg1 + jfk,"%d",temp-1);
                jfk += cnt;
                arg1[jfk] = '.';

            }
            int reflen = jfk;
            // for(int j = 0; j <r2; ++j){
            //     printf("%c",arg1[j]);
            //     printf("%3d",j);
            //     }
            printf("\n");
            char arg2[10], arg3[10], arg4[10], arg5[10];
            sprintf(arg2, "%d", MQ1);
            sprintf(arg3, "%d", MQ3);
            sprintf(arg4, "%d", i);
            sprintf(arg5, "%d", reflen);
            process_totalpage[i] = r1;
            // if(i == 1)
            //     process_totalpage[i] = 4;

            process_totalframe[i] = r1;
            printf("Master: Process %d created with %d pages\n", i, process_totalpage[i]);
            execl("./process", "./process", arg1, arg2, arg3, arg4, arg5, NULL);
            exit(0);
        }
        else if (pid < 0)
        {
            perror("fork failed");
            exit(1);
        }
        usleep(250); // Sleep for 250 ms
    }

    
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = -1;
    sb.sem_flg = 0;

    int semid = mtx_m_sch;
    semop(semid, &sb, 1);

    kill(mmu_pid, SIGKILL);
    kill(sched_id, SIGKILL);

    // Cleanup and exit
    
    // remove semaphore
    for (int i = 0; i < k; i++)
    {
        semctl(mtx[i], 0, IPC_RMID);
    }
    semctl(mtx_mmu_p, 0, IPC_RMID);

    // Detach and remove shared memory for page tables
    shmdt(page_tables);
    shmctl(SM1, IPC_RMID, NULL);

    // Detach and remove shared memory for free frame list
    shmdt(free_frame_list);
    shmctl(SM2, IPC_RMID, NULL);

    //other
    shmdt(shared_resource);
    shmctl(other, IPC_RMID, NULL);

    shmdt(process_totalframe);
    shmctl(ptf, IPC_RMID, NULL);

    shmdt(process_totalpage);
    shmctl(ptp, IPC_RMID, NULL);

    // Remove message queues
    msgctl(MQ1, IPC_RMID, NULL);
    msgctl(MQ2, IPC_RMID, NULL);
    msgctl(MQ3, IPC_RMID, NULL);

    return 0;
}
