#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <semaphore.h>

#define NPROCS 4
#define SERIES_MEMBER_COUNT 200000

double *sums;
double x = 1.0;
int *proc_count;
int *start_all;
double *res;

sem_t mutex; // Semáforo para la exclusión mutua
sem_t all_done; // Semáforo para indicar que todos los esclavos han terminado

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
    sem_wait(&mutex); // Entrar en la región crítica
    sem_post(&mutex); // Salir de la región crítica
    for(i = proc_num; i < SERIES_MEMBER_COUNT; i += NPROCS)
        sums[proc_num] += get_member(i + 1, x);
    
    sem_wait(&mutex); // Entrar en la región crítica
    (*proc_count)++;
    if (*proc_count == NPROCS) // Si este es el último esclavo, señalar que todos han terminado
        sem_post(&all_done);
    sem_post(&mutex); // Salir de la región crítica
    
    exit(0);
}

void master_proc() {
    sem_wait(&all_done); // Esperar a que todos los esclavos terminen
    *res = 0;
    for(int i = 0; i < NPROCS; i++)
        *res += sums[i];
    
    exit(0);
}

int main() {
    int p;
    int shmid;
    void *shmstart;

    // Inicializar semáforos
    sem_init(&mutex, 1, 1); // mutex se inicializa en 1 para la exclusión mutua
    sem_init(&all_done, 1, 0); // all_done se inicializa en 0 para esperar a que todos los esclavos terminen
    
    shmid = shmget(0x1234, NPROCS * sizeof(double) + 2 * sizeof(int), 0666 | IPC_CREAT);
    shmstart = shmat(shmid, NULL, 0);
    sums = shmstart;
    proc_count = shmstart + NPROCS * sizeof(double);
    start_all = shmstart + NPROCS * sizeof(double) + sizeof(int);
    res = shmstart + NPROCS * sizeof(double) + 2 * sizeof(int);
    *proc_count = 0;
    *start_all = 0;

    for(int i = 0; i < NPROCS; i++) {
        p = fork();
        if(p == 0)
            proc(i);
    }

    p = fork();
    if(p == 0)
        master_proc();

    printf("El recuento de ln(1 + x) miembros de la serie de Mercator es %d\n", SERIES_MEMBER_COUNT);
    printf("El valor del argumento x es %f\n", (double)x);

    for(int i = 0; i < NPROCS + 1; i++)
        wait(NULL);

    printf("El resultado es %10.8f\n", *res);
    printf("Llamando a la función ln(1 + %f) = %10.8f\n", x, log(1 + x));

    shmdt(shmstart);
    shmctl(shmid, IPC_RMID, NULL);

    // Destruir semáforos
    sem_destroy(&mutex);
    sem_destroy(&all_done);

    return 0;
}
