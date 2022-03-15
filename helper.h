/******************************************************************
 * Header file for the helper functions. This file includes the
 * required header files, as well as the function signatures and
 * the semaphore values (which are to be changed as needed).
 ******************************************************************/


# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <sys/types.h>
# include <sys/ipc.h>
# include <sys/shm.h>
# include <sys/sem.h>
# include <sys/time.h>
# include <math.h>
# include <errno.h>
# include <string.h>
# include <pthread.h>
# include <ctype.h>
# include <iostream>
using namespace std;

# define SEM_KEY 0x56 // Change this number as needed
# define SEM_NUM 3 // Number of semaphores to create
# define QUEUE_MUTEX_SEM 0// Semaphore array index for queue mutex
# define SPACE_SEM 1 // Semaphore array index for space semaphore
# define JOBS_SEM 2 // Semaphore array index for jobs semaphore

union semun {
    int val;               /* used for SETVAL only */
    struct semid_ds *buf;  /* used for IPC_STAT and IPC_SET */
    ushort *array;         /* used for GETALL and SETALL */
};

int check_arg (char *);
int sem_create (key_t, int);
int sem_init (int, int, int);
void sem_wait (int, short unsigned int);
int sem_wait_twenty_secs(int id, short unsigned int num);
void sem_signal (int, short unsigned int);
int sem_close (int);
