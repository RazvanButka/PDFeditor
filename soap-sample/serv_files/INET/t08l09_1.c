/**
Lab. 09 Tema 08
Titlul temei: Sincronizarea thread-urilor pe variabile de condiție
Data livrarii: 19 Apr. 2022
Student: Botescu Mihai-Alexandru
An 3 IA, Sg1
----------
<
Programul a fost rulat sub macOS versiunea 12.0
Informatii suplimentare:
BuildVersion:   21A5552a
Am considerat ca este nevoie de o serie de functii auxiliare, cum ar fi:

--> cate una pentru fiecare situatie a problemei in parte
    -- daca am numar par de thread-uri, se afiseaza niste mesaje tip text
    -- daca am numar impar de thread-uri, se afiseaza pur si simplu o variabila simpla incrementata la fiecare pas
--> cate o functie handlera pentru thread-uri pentru fiecare din cele 2 situatii de mai sus
--> o functie ajutatoare pentru parsarea argumentelor de pe linia de c-da.

Codul este rulat in main.


Functionalitatea programului este urmatoarea:
Se creeaza (functie de valoarea argumentului primit pe linia de c-da),
un numar de thread-uri. In functie de acest lucru, se va intra sau pe
primul caz (afisarea unor mesaje text) sau pe al doilea (afisarea si
incrementarea unei variabile simple).
Este verificat numarul de argumente, corectitudinea transmiterii optiunii
si valorii acesteia, precum si esecul apelurilor sistem.
In cazul in care nu sunt indeplinite conditiile de mai sus (exceptie nr de argumente)
,programul incheie prematur executia cu un cod ce semnifica eroare (EXIT_FAILURE).
Daca numarul de argumente este prea mic, se va folosi o valoare default pt nr de
thread-uri.
>
**/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

/* the default threads number, in case of not providing the argument on the CLI. */
#define DEFAULT_THREAD_NO 6

/* the condition variable. */
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

/* the mutex lock we're going to use. */
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;


/* the messages we're going to print. */
static const char *messages[] = {
        "este inca o zi",
        "pierduta",
        "pe bancile",
        "statului"
};

/* a variable for accessing the current message. */
static volatile sig_atomic_t idx;

/* first kind of thread's function. */
void *tfunc1(void *targ) {
    /* acquire the mutex lock. */
    if (pthread_mutex_lock(&mtx)) { /* in case of error, exit. */
        fprintf(stderr, "Error acquiring a mutex lock on the thread!\n");
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
    /* critical section begins here. */
    /* check the index of the message. */
    switch (idx % 2) {
        case 0: /* if it's odd, then print it, raise the index, and do the following. */
            fprintf(stdout, "%s\n", messages[idx]);
            idx++;
            /* wait on the condition variable. */
            /* in case of error, exit. */
            if (pthread_cond_wait(&cond, &mtx)) {
                fprintf(stderr, "Error waiting on the condition variable on the thread!\n");
                perror("pthread_cond_wait");
                exit(EXIT_FAILURE);
            }
            break; /* exit. */
        case 1: /* in other cases, signal the condition variable. */
            if (pthread_cond_signal(&cond)) { /* in case of error, exit. */
                fprintf(stderr, "Error signaling on the condition variable on the thread!\n");
                perror("pthread_cond_signal");
                exit(EXIT_FAILURE);
            }
            fprintf(stdout, "%s\n", messages[idx]); /* print the current message. */
            /* if we reached the end of the messages array, reset the index and start from scratch. */
            idx = (idx == sizeof(messages) / sizeof(char *) - 1) ? 0 : idx + 1;
            break; /* exit. */
    }
    /* unlock the mutex. */

    if (pthread_mutex_unlock(&mtx)) {  /* in case of error, exit. */
        fprintf(stderr, "Error releasing the mutex lock on the thread!\n");
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }

    /* return something, because the return type is void*. */
    return targ ? targ : NULL;
}


/* the demo variable used. */
static volatile int global = 1;

/* handler for the second kind of threads. */
void *tfunc2(void *targ) {
    /* acquire the mutex lock. */
    if (pthread_mutex_lock(&mtx)) {
        fprintf(stderr, "Error acquiring a mutex lock on the thread!\n");
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
    /* critical section begins here. */
    /* check the variable. */
    if (global % 2 == 0) { /* if it's odd, wait on the cond variable. */
        fprintf(stdout, "[%d]\tglobal=%d\n", global, global); /* print it's value. */
        global++; /* move on with the variable. */
        if (pthread_cond_wait(&cond, &mtx)) { /* in case of error, exit. */
            fprintf(stderr, "Error waiting on the condition variable on the thread!\n");
            perror("pthread_cond_wait");
            exit(EXIT_FAILURE);
        }
    } else if (global % 2 == 1) {/* in other cases, signal the condition variable. */
        if (pthread_cond_signal(&cond)) {/* in case of error, exit. */
            fprintf(stderr, "Error signaling on the condition variable on the thread!\n");
            perror("pthread_cond_signal");
            exit(EXIT_FAILURE);
        }
        fprintf(stdout, "[%d]\tglobal=%d\n", global, global); /* print the variable. */
    }
    global++; /* move on to the next value. */
    /* critical section ends here. */
    /* unlock the mutex. */
    if (pthread_mutex_unlock(&mtx)) {/* in case of error, exit. */
        fprintf(stderr, "Error releasing the mutex lock on the thread!\n");
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
    /* return something, because the return type is void*. */
    return !targ ? NULL : targ;
}

/* helper function to parse the CLI arguments options. */
char *getarg(int argc, char *const argv[]) {
    char c;
    int opt;
    /* find the -t option. */
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
            case 't':
                return optarg; /* if found, return it's mapped value. */
                break;
        }
    }
    return NULL; /* if not found, do not return anything. Or return NULL :) */
}


/* helper function for the first case, odd number of threads. */
static void runA(pthread_t *tid, long loops) {
    int i;
    for (i = 0; i < loops; i++) { /* create the threads. indicate, for each one, the handler function. */
        if (pthread_create(&tid[i], NULL, tfunc1, NULL)) {/* in case of error, exit. */
            fprintf(stderr, "Error during creating thread [%d]!\n", i + 1);
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    /* wait for the threads to finish. */
    for (i = 0; i < loops; i++) {
        if (pthread_join(tid[i], NULL)) {/* in case of error, exit. */
            fprintf(stderr, "Error during joining thread [%d]!\n", i + 1);
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }
}

/* helper function for the second case, even number of threads. */
static void runB(pthread_t *tid, long loops) {
    int i;
    for (i = 0; i < loops; i++) {
        if (pthread_create(&tid[i], NULL, tfunc2,
                           NULL)) { /* create the threads. indicate, for each one, the handler function. */
            fprintf(stderr, "Error during creating thread [%d]!\n", i + 1); /* in case of error, exit. */
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    /* wait for the threads to finish. */
    for (i = 0; i < loops; i++) {
        if (pthread_join(tid[i], NULL)) { /* in case of error, exit. */
            fprintf(stderr, "Error during joining thread [%d]!\n", i + 1);
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }
}

/* main function. */
int main(int argc, char *const argv[]) {
    /* get the thread no from the CLI, or use the default THREAD_NO. */
    long loops = (argc > 2) ? strtol(getarg(argc, argv), NULL, 10) : DEFAULT_THREAD_NO;
    /* check if loops were converted successfully to numeric data. */
    if (loops <= 0) {/* in case of error, exit. */
        fprintf(stderr, "Error on converting argument to numeric data!\n");
        perror("strtol");
        exit(EXIT_FAILURE);
    }

    /* creating how many threads? */
    fprintf(stdout, "Creating %ld threads\n", loops);

    /* declare and malloc the thread pool. */
    pthread_t *tid = (pthread_t *) malloc(loops * sizeof(pthread_t));
    if (!tid) { /* in case of error, exit. */
        fprintf(stderr, "Error malloc!\n");
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    switch (loops % 2) { /* the number of threads. */
        case 0:
            runA(tid, loops);
            break;
        case 1:
            runB(tid, loops);
            break;
    }
    /* if no errors occured, exit success. */
    exit(EXIT_SUCCESS);
}
/** Exemple de compilare si executie ale programului.
* gcc -o t08l09_1{,.c} && ./t08l09_1 -t 5                                    
Creating 5 threads
[1]     global=1
[2]     global=2
[3]     global=3
[4]     global=4
[5]     global=5

* gcc -o t08l09_1{,.c} && ./t08l09_1 -t 4                                    
Creating 4 threads
este inca o zi
pierduta
pe bancile
statului


 gcc -o t08l09_1{,.c} && ./t08l09_1                                         
Creating 6 threads
este inca o zi
pierduta
pe bancile
statului
este inca o zi
pierduta


* gcc -o t08l09_1{,.c} && ./t08l09_1 -t anamaria                             
Error on converting argument to numeric data!
strtol: Invalid argument




*/