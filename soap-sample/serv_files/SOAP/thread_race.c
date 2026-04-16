#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

static long global;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

void* tfunc(void* arg)
{
	long n = *((long*)arg);
	long i;
	long local;

	for (i = 0; i < n; i++)
	{
        if (pthread_mutex_lock(&mtx) != 0)
		{
			fprintf(stderr, "Error locking mutex!\n");
			exit(4);
		}
		local = global;
		local++;
		global = local;
        if (pthread_mutex_unlock(&mtx) != 0)
		{
			fprintf(stderr, "Error unlocking mutex!\n");
			exit(5);
		}
	}
	
	return NULL;
}

int main(int argc, char* argv[])
{
	pthread_t tid[2];
	
	int i;
	long loops = 100;
	/*
	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s loopNo\n", argv[0]);
		exit(1);
	} */

//	loops = strtol(argv[1], NULL, 10);
	for (i = 0; i < 2; i++)
		if (pthread_create(&(tid[i]), NULL, tfunc, &loops) != 0)
		{
			fprintf(stderr, "Failed to create new thread no. %d!\n", i);
			exit(2);
		}
	
	for (i = 0; i < 2; i++)
		if (pthread_join(tid[i], NULL) != 0)
		{
			fprintf(stderr, "pthread_join returned an error for thread no. %d!\n", i);
			exit(3);
		}
	
	printf("%ld\n", global);
	return 0;
}
