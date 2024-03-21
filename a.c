#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define NPROCS 4
#define SERIES_MEMBER_COUNT 200000

double *sums;
double x = 1.0;

int *proc_count;
int *start_all;
double *res;

int semid;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
} semopts;

void sem_wait(int semid) {
    struct sembuf sembuf = {0, -1, SEM_UNDO};
    semop(semid, &sembuf, 1);
}

void sem_signal(int semid) {
    struct sembuf sembuf = {0, 1, SEM_UNDO};
    semop(semid, &sembuf, 1);
}

double get_member(int n, double x) {
    int i;
    double numerator = 1;

    for(i = 0; i < n; i++)
        numerator = numerator * x;

    if (n % 2 == 0)
        return (-numerator / n);
    else
        return numerator / n;
}

void proc(int proc_num) {
    int i;
    sem_wait(semid);
    sums[proc_num] = 0;
    for (i = proc_num; i < SERIES_MEMBER_COUNT; i += NPROCS)
        sums[proc_num] += get_member(i + 1, x);

    (*proc_count)++;
    sem_signal(semid);
    exit(0);
}

void master_proc() {
    int i;
    sleep(1);
    *start_all = 1;

    while (*proc_count != NPROCS) {}

    sem_wait(semid);
    *res = 0;
    for (i = 0; i < NPROCS; i++)
        *res += sums[i];
    sem_signal(semid);

    exit(0);
}

int main() {
    int shmid;
    void *shmstart;
    int p;
    int i;

    shmid = shmget(0x1234, NPROCS * sizeof(double) + 2 * sizeof(int), 0666 | IPC_CREAT);
    shmstart = shmat(shmid, NULL, 0);
    sums = shmstart;
    proc_count = shmstart + NPROCS * sizeof(double);
    start_all = shmstart + NPROCS * sizeof(double) + sizeof(int);
    res = shmstart + NPROCS * sizeof(double) + 2 * sizeof(int);

    semid = semget(IPC_PRIVATE, 1, 0666 | IPC_CREAT);
    semopts.val = 0;
    semctl(semid, 0, SETVAL, semopts);

    *proc_count = 0;
    *start_all = 0;

    for (i = 0; i < NPROCS; i++) {
        p = fork();
        if (p == 0)
            proc(i);
    }

    p = fork();
    if (p == 0)
        master_proc();

    for (i = 0; i < NPROCS + 1; i++)
        wait(NULL);

    printf("El recuento de ln(1 + x) miembros de la serie de Mercator es %d\n", SERIES_MEMBER_COUNT);
    printf("El valor del argumento x es %f\n", (double)x);
    printf("El resultado es %10.8f\n", *res);
    printf("Llamando a la función ln(1 + %f) = %10.8f\n", x, log(1 + x));

    shmdt(shmstart);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID, semopts);
    
    return 0;
}
