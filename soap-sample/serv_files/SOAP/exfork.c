/*****************************
* FISIER: process_chains_0.c
*****************************/
#include<stdio.h>
#include<sys/types.h>
#include<unistd.h>
int main()
{
int i=0; // i= numar proces creat
int n=4; // n-1= numar procese din lant
pid_t childpid;
printf("\n%i Acesta este procesul initial cu PID=%ld care are parintele PPID=%ld\n", i, (long) getpid(),
(long)getppid());
for(i=1; i<n; ++i) if (childpid = fork()) break; // in parinte terminare fortata bucla for
else{
/* secventa proces copil */
if(i == 1)sleep(1); // !!! intarziere pt. ca parintele sa se termine inaintea copilului
printf("\n%i Acesta este procesul creat cu PID=%ld care are parintele PPID=%ld\n", i, (long)
getpid(), (long)getppid());
}
/* secventa comuna proces parinte/copil */
return 0;
}
