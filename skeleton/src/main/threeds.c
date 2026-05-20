#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "pdf_server.h"

void *unix_main(void *args);
void *inet_main(void *args);
void *soap_main(void *args);
void *pdf_main(void *args);

#define UNIXSOCKET "/tmp/unixds"
#define INETPORT 18081
#define SOAPPORT 18082
#define PDF_PORT 18083

pthread_mutex_t curmtx = PTHREAD_MUTEX_INITIALIZER;

static void usage(const char *prog)
{
    (void)fprintf(stderr,
                  "Utilizare: %s [-c config.cfg] [-p pdf_port]\n",
                  prog);
}

int main(int argc, char *argv[])
{
    int iport = INETPORT;
    int sport = SOAPPORT;
    int pport = PDF_PORT;
    int opt;
    const char *configFile = NULL;

    while ((opt = getopt(argc, argv, "c:p:h")) != -1)
    {
        switch (opt)
        {
        case 'c':
            configFile = optarg;
            pdf_server_set_config_path(optarg);
            break;
        case 'p':
        {
            long p = strtol(optarg, NULL, 10);
            if (p > 0 && p <= 65535)
            {
                pport = (int)p;
            }
            break;
        }
        case 'h':
        default:
            usage(argv[0]);
            return (opt == 'h') ? EXIT_SUCCESS : EXIT_FAILURE;
        }
    }

    if (configFile != NULL)
    {
        pdf_server_set_config_path(configFile);
    }

    pthread_t unixthr;
    pthread_t inetthr;
    pthread_t soapthr;
    pthread_t pdfthr;

    (void)unlink(UNIXSOCKET);

    pthread_create(&unixthr, NULL, unix_main, (void *)UNIXSOCKET);
    pthread_create(&inetthr, NULL, inet_main, &iport);
    pthread_create(&soapthr, NULL, soap_main, &sport);
    pthread_create(&pdfthr, NULL, pdf_main, &pport);

    pthread_join(unixthr, NULL);
    pthread_join(inetthr, NULL);
    pthread_join(soapthr, NULL);
    pthread_join(pdfthr, NULL);

    (void)unlink(UNIXSOCKET);
    return 0;
}
