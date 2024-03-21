#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <pthread.h> 

#define NPROCS 4
#define SERIES_MEMBER_COUNT 1000

double *sums; // Puntero para almacenar las sumas parciales de los hilos
double x = 1.0;

int *proc_count; // Contador de hilos que han completado su tarea
int *start_all; // Bandera para indicar que todos los hilos han iniciado
double *res; // Resultado final de la suma de las sumas parciales
pthread_mutex_t lock_master; // Mutex para sincronizar el proceso maestro
pthread_mutex_t lock_proc; // Mutex para sincronizar los procesos hijo

// Función para calcular un miembro de la serie de Mercator
double get_member(int n, double x){
    int i;
    double numerator = 1;

    for(i = 0; i < n; i++ )
        numerator = numerator * x;

    if (n % 2 == 0)
        return (-numerator / n);
    else
        return numerator / n;
}

// Función ejecutada por cada hilo
void proc(int proc_num){
    int i;
    pthread_mutex_lock(&lock_proc); // Bloquea el mutex para sincronizar el acceso al arreglo de sumas
    sums[proc_num] = 0;
    for(i = proc_num; i < SERIES_MEMBER_COUNT; i += NPROCS)
        sums[proc_num] += get_member(i + 1, x);

    (*proc_count)++; // Incrementa el contador de hilos que han completado su tarea
    pthread_mutex_unlock(&lock_master); // Desbloquea el mutex para permitir que el proceso maestro continúe
    exit(0);
}

// Función ejecutada por el proceso maestro
void master_proc(){
    int i;
    sleep(1);
    *start_all = 1; // Establece la bandera para indicar que todos los hilos han iniciado
    for(int j = 0; j < NPROCS; j++){
        pthread_mutex_unlock(&lock_proc); // Desbloquea el mutex para permitir que los procesos hijo continúen
    }
    for(int j = 0; j < NPROCS; j++){
        pthread_mutex_lock(&lock_master); // Bloquea el mutex para sincronizar el acceso al resultado final
    }

    *res = 0;
    for(i = 0; i < NPROCS; i++)
        *res += sums[i]; // Suma las sumas parciales calculadas por los hilos
    
    exit(0);
}

// Main
int main(){
    int *threadIdPtr;
    long long start_ts;
    long long stop_ts;
    long long elapsed_time;
    long lElapsedTime;
    struct timeval ts;
    int i;
    int p;
    int shmid;
    void *shmstart;

    // Crea un segmento de memoria compartida
    shmid = shmget(0x1234, NPROCS * sizeof(double) + 2 * sizeof(int), 0666 | IPC_CREAT);
    shmstart = shmat(shmid, NULL, 0); // Adjunta el segmento de memoria compartida al espacio de direcciones del proceso
    sums = shmstart;
    proc_count = shmstart + NPROCS * sizeof(double);
    start_all = shmstart + NPROCS * sizeof(double) + sizeof(int);
    res = shmstart + NPROCS * sizeof(double) + 2 * sizeof(int);
    *proc_count = 0;
    *start_all = 0;

    // Inicializa los mutex
    pthread_mutex_init(&lock_master, NULL);
    pthread_mutex_init(&lock_proc, NULL);

    gettimeofday(&ts, NULL);
    start_ts = ts.tv_sec; // Tiempo inicial
    for(i = 0; i < NPROCS; i++){
        p = fork(); // Crea un nuevo proceso hijo
        if(p == 0)
            proc(i); // Si es el proceso hijo, ejecuta la función proc
    }
    p = fork();
    if(p == 0)
        master_proc(); // Si es el proceso hijo, ejecuta la función master_proc

    // Imprime información sobre la serie de Mercator y el valor del argumento x
    printf("El recuento de ln(1 + x) miembros de la serie de Mercator es %d\n", SERIES_MEMBER_COUNT);
    printf("El valor del argumento x es %f\n", (double)x);
    for(int i = 0; i < NPROCS + 1; i++)
        wait(NULL); // Espera a que todos los procesos hijos terminen

    gettimeofday(&ts, NULL);
    stop_ts = ts.tv_sec; // Tiempo final
    elapsed_time = stop_ts - start_ts;
    printf("Tiempo = %lld segundos\n", elapsed_time);
    printf("El resultado es %10.8f\n", *res);
    printf("Llamando a la función ln(1 + %f) = %10.8f\n", x, log(1 + x));
    
    // Desadjunta y elimina el segmento de memoria compartida
    shmdt(shmstart);
    shmctl(shmid, IPC_RMID, NULL);
    
    // Destruye los mutex
    pthread_mutex_destroy(&lock_master);
    pthread_mutex_destroy(&lock_proc);

    return 0;
}
