/**
Lab. 10 Tema 09 #2
Titlul temei: Rezolvarea deadlock-ului pe conditie de excludere mutuala (concurent, cu procese).
Data livrarii: 3 Mai 2022
Student: Botescu Mihai-Alexandru
An 3 IA, Sg1
----------
<
Programul a fost rulat sub macOS versiunea 12.0
Informatii suplimentare:
BuildVersion:   21A5552a

 Nu am considerat ca este nevoie de vreo functie ajutatoare definita de mine.
Codul este rulat in main.


Functionalitatea programului este urmatoarea:
Se creeaza 2 procese (in lant).
Acestea comunica intre ele printr-un segment de memorie partajata.
Pentru aceasta, am definit o structura separata.
Aceasta structura definita de mine contine:
 * O variabila simpla, local (simulare lucru pentru program :D)
 * 2 mutex locks: m, m1
 * 2 atribute pentru mutex (utile pentru a specifica comunicarea intre procese prin segment de memorie partajata)

Rezolvarea deadlock-ului are loc in felul urmator:
 * in unul din procese, se blocheaza m, se deblocheaza m si se blocheaza m1 (in aceasta ordine)
                --- intre ele se face un sleep! pentru a ajuta la sincronizare...
                --- de asemenea, se simuleaza operatii simple pe variabila simpla
 * in celalalt proces, se blocheaza m1, se deblocheaza m1 si se blocheaza m (in aceasta ordine)
                --- intre ele se face un sleep! pentru a ajuta la sincronizare...
                --- de asemenea, se simuleaza operatii simple pe variabila simpla
Au fost verificate esecul apelurilor sistem, precum si esecul altor functii utilizate pe parcurs.
>
**/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <sys/wait.h>

#define FISIER "t09_l10_1"
#define Pr_ID_1 1
#define Pr_ID_2 2
typedef struct mem_partajata {
    int local;
    pthread_mutex_t m;
    pthread_mutex_t m1;
    pthread_mutexattr_t matr, matr1;
} mem;

int main(void) {
    mem *shm_ptr_zona;
    key_t shm_key;
    int shm_id;
    struct shmid_ds shmid_ds, *shm_id_ds; // Structura de control segment memorie partajata
    shm_id_ds = &shmid_ds;
    shm_key = ftok(FISIER, Pr_ID_1);
    if ((shm_id = shmget((key_t) shm_key, sizeof(mem), IPC_CREAT | 0666)) == -1) {
        (perror("err. shmget()\n"));
        exit(EXIT_FAILURE);
    }
    shmctl(shm_id, IPC_STAT, shm_id_ds);
    shm_ptr_zona = (mem *) shmat(shm_id, NULL, 0);
    pthread_mutexattr_init(&shm_ptr_zona->matr);
    pthread_mutexattr_init(&shm_ptr_zona->matr1);
    pthread_mutexattr_setpshared(&shm_ptr_zona->matr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setpshared(&shm_ptr_zona->matr1, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shm_ptr_zona->m1, &shm_ptr_zona->matr);
    pthread_mutex_init(&shm_ptr_zona->m, &shm_ptr_zona->matr1);
    if ((void *) shm_ptr_zona == (void *) -1) {
        perror("err. shmat");
        shmctl(shm_id, IPC_RMID, NULL); // Executa operatia de control IPC_RMID
// (marcheaza segmentul pentru a fi distrus)

        exit(EXIT_FAILURE);
    }
    shmctl(shm_id, IPC_STAT, shm_id_ds);
    pid_t childpid;
    int status;
    childpid = fork();
    if (childpid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (childpid == 0) {
        fprintf(stderr, "P1 vrea R1\n");
        pthread_mutex_lock(&shm_ptr_zona->m);
        fprintf(stderr, "P1 obtine R1\n");
        key_t key;
        // usleep(200);
        shm_ptr_zona->local++;
        pthread_mutex_unlock(&shm_ptr_zona->m);
        int mid;
        key = ftok(FISIER, Pr_ID_1);
        fprintf(stderr, "P1 vrea R2\n");
        pthread_mutex_lock(&shm_ptr_zona->m1);
        fprintf(stderr, "P1 obtine R2\n");
        shm_ptr_zona->local++;
    } else {
        fprintf(stderr, "P2 vrea R2\n");
        pthread_mutex_lock(&shm_ptr_zona->m1);
        fprintf(stderr, "P2 obtine R2\n");
        usleep(200);
        shm_ptr_zona->local--;
        pthread_mutex_unlock(&shm_ptr_zona->m1);
        key_t key;
        int mid;
        fprintf(stderr, "P2 vrea R1\n");
        pthread_mutex_lock(&shm_ptr_zona->m);
        shm_ptr_zona->local--;
        fprintf(stderr, "P2 obtine R1\n");
    }
    wait(NULL);
    return EXIT_SUCCESS;
}

/**  Output obtinut:
* P2 vrea R2
P2 obtine R2
P1 vrea R1
P1 obtine R1
P2 vrea R1
P2 obtine R1
P1 vrea R2
P1 obtine R2

 *
*/