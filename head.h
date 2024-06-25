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


typedef struct pgelem
{
    int frame_number;
    int valid_bit;
    // int tframe;
} PageTableEntry;

typedef struct 
{
    int k, m, f;
    int current_process;
} Shared_struct;

typedef struct
{
    long msgtype;
    int value;
} message;

typedef struct 
{
    int invalid_refs;
    int page_faults;
} proc_log;

#define KEY 57
#define KEY2 100
#define KEY3 59
#define KEY4 60
#define KEY5 61
#define KEY6 62
#define KEY7 63
#define KEY8 64
#define KEY9 65
#define KEY10 66