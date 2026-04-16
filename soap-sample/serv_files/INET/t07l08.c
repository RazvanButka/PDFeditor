/**
Lab. 07 Tema 7
Titlul temei: Tehnica de comunicare IPC prin semnale
Data livrarii: 10.04.2022
Student: Botescu Mihai-Alexandru
An 3 IA, Sg1
----------
<
Programul a fost rulat sub macOS versiunea 12.0
Informatii suplimentare:
BuildVersion:   21A5552a.
Functionalitatea programului este urmatoarea:
 Programul C nu primeste niciun argument de pe linia de comanda.
 Daca sunt primite unul sau mai multe argumente, ele vor fi ignorate.
 Se creeaza un proces fiu al procesului curent.
 Procesul parinte asteapta ca fiul sa-i trimita semnalul SIGUSR2.
 Dupa interceptare, parintele trimite fiului semnalului SIGINT,
 pentru a il forta sa incheie executia.
 Manipularea semnalului SIGUSR2 a fost realizata in program, in timp ce
 manipularea semnalului SIGINT a fost lasata in seama sistemului de operare.
 Pentru semnalul SIGUSR2, a fost definita o functie handler de catre utilizator.
 In fiu, are loc un for-loop infinit, in care (de test) se afiseaza un mesaj
 la fiecare pas si se face sleep(1) pentru a nu se afisa prea multe mesaje,
 pana cand SIGINT a fost interceptat de catre fiu.
 La interceptarea semnalului SIGUSR2, de asemenea, se va afisa un mesaj
 de informare privind interceptarea acestuia.
 In final, dupa incheierea executiei fiului,
 parintele opreste executia intregului program.

 Au fost verificate:
 * Esecul apelurilor sistem utilizate
 * Esecul altor functii utilizate pe parcurs.

 In caz de eroare, se vor afisa mesaje pe parcurs care sa informeze utilizatorul, si se va incheia prematur executia programului, cu un cod de exit ce reprezinta FAILURE (EXIT_FAILURE).

>
**/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

/* define the signal variable (flag) we're going to use in order to mark the reception of the signal. */
static volatile sig_atomic_t sig;
static int sigiter;

/* the SIGUSR2 signal handler function. */
void SIGUSR2_handler(int signum) {
    if (signum == SIGUSR2) {
        fprintf(stdout, "Got signal: %s\n", strsignal(signum));
        sig = signum;
    }
}

/* main program */
int main(int argc, char **argv) {
    static struct sigaction sa;
    /* Empty mask, initialize it. */
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGUSR2);
    sigaddset(&sa.sa_mask, SIGINT);
    sigfillset(&sa.sa_mask);
    /* Initialize the flags. */
    sa.sa_flags = 0;
    /* process the SIGUSR2 signal. */
    sa.sa_handler = SIGUSR2_handler;
    if (sigaction(SIGUSR2, &sa, NULL) == -1) {
        fprintf(stderr, "Error on sigaction for SIGUSR2!\n");
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    pid_t p;
    if (argc > 1) {
        fprintf(stderr, "I will ignore everything you pass as CL args.\n");
    }
    int status;
    /* make a new child process. */
    switch (p = fork()) {
        /* in case of error, exit. */
        case -1:
            perror("fork");
            exit(EXIT_FAILURE);
        case 0: /* this is the child. */
            /* send the sigusr2 signal to the parent. */
            if (kill(getppid(), SIGUSR2) == -1) { /* if this fails, print a message and exit. */
                fprintf(stderr, "Error while sending SIGUSR2 from child to parent!\n");
                perror("kill");
                exit(EXIT_FAILURE);
            }
            /* perform an infinite for loop. */
            for (;;) {
                fprintf(stderr, "Loop\n"); /* for testing purposes only. */
                sleep(1); /* delay for one second so we don't obtain much output. */
            }
            return 0; /* exit the child process. */
            break;
        default: /* this is the parent. */
            while (sig != SIGUSR2) {/* wait for SIGUSR2 to be received. */
                if (!sigiter) /* print the message only once. */
                    fprintf(stderr, "Waiting for SIGUSR2...\n");
                ++sigiter;
            }
            if (kill(p, SIGINT) == -1) { /* send SIGINT to child. if this fails, print an error message and exit. */
                fprintf(stderr, "Error while sending SIGINT to child!\n");
                perror("kill");
                exit(EXIT_FAILURE);
            }
            sigsuspend(&sa.sa_mask);
            if (wait(&status) == -1) { /* wait for the child to finish it's execution. */
                fprintf(stderr, "Error while waiting for child!\n");
                perror("wait");
                exit(EXIT_FAILURE);
            }
            return 0; /* exit the parent process. */
            break;
    }
    exit(EXIT_SUCCESS); /* if everything happens normally, then exit with SUCCESS code. */
}

/** Exemplu de compilare si executie a programului
*  gcc -o t07l08{,.c} && ./t07l08                                             
Waiting for SIGUSR2...
Loop
Got signal: User defined signal 2: 31


*  gcc -o t07l08{,.c} && ./t07l08 5                                           
I will ignore everything you pass as CL args.
Waiting for SIGUSR2...
Loop
Got signal: User defined signal 2: 31

*/