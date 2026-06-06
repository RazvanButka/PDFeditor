
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "pandoc_convert.h"


static int waitWithTimeout(pid_t pid, int timeoutSec, int *wstatus)
{
    const int POLL_INTERVAL_MS = 100;
    const int MAX_POLLS = (timeoutSec * 1000) / POLL_INTERVAL_MS;

    for (int i = 0; i < MAX_POLLS; i++) {
        pid_t r = waitpid(pid, wstatus, WNOHANG);
        if (r == pid) {
            return 0; 
        }
        if (r < 0 && errno == ECHILD) {
            *wstatus = 0;
            return 0;
        }
        if (r < 0) {
            return -2;
        }

        struct timespec ts = {0, POLL_INTERVAL_MS * 1000000L};
        (void)nanosleep(&ts, NULL);
    }

    (void)fprintf(stderr, "[pandoc] Timeout %d sec depasit, SIGKILL pid=%d\n",
                  timeoutSec, pid);
    (void)kill(pid, SIGKILL);

    (void)waitpid(pid, wstatus, 0);
    return -1;
}



int pandoc_is_available(void)
{

    pid_t pid = fork();
    if (pid < 0) {
        return 0;
    }

    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            (void)dup2(devnull, STDOUT_FILENO);
            (void)dup2(devnull, STDERR_FILENO);
            (void)close(devnull);
        }
        char *argv[] = {"pandoc", "--version", NULL};
        execvp("pandoc", argv);
        _exit(127);  
    }

    int wstatus;
    if (waitWithTimeout(pid, 5, &wstatus) != 0) {
        return 0;
    }
    return WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0;
}


int pandoc_format_lookup(int outFmt, const char **pandocFmt, const char **extension)
{
    switch (outFmt) {
        case 2: 
            *pandocFmt = "docx";
            *extension = "docx";
            return 1;
        case 3: 
            *pandocFmt = "html";
            *extension = "html";
            return 1;
        case 4:
            *pandocFmt = "markdown";
            *extension = "md";
            return 1;
        case 5: 
            *pandocFmt = "rtf";
            *extension = "rtf";
            return 1;
        default:
            *pandocFmt = NULL;
            *extension = "txt";
            return 0;
    }
}


int pandoc_convert(const char *inputPath,
                   const char *outputPath,
                   const char *fromFormat,
                   const char *toFormat,
                   int timeoutSec)
{
    /* Validare parametri */
    if (inputPath == NULL || outputPath == NULL ||
        fromFormat == NULL || toFormat == NULL) {
        (void)fprintf(stderr, "[pandoc] Parametru NULL\n");
        return -1;
    }
    if (timeoutSec <= 0 || timeoutSec > 600) {
        (void)fprintf(stderr, "[pandoc] Timeout %d invalid (1-600 sec)\n",
                      timeoutSec);
        return -1;
    }
    if (access(inputPath, R_OK) != 0) {
        (void)fprintf(stderr, "[pandoc] Input '%s' indisponibil: %s\n",
                      inputPath, strerror(errno));
        return -1;
    }

    sigset_t blockMask, oldMask;
    sigemptyset(&blockMask);
    sigaddset(&blockMask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &blockMask, &oldMask);

    pid_t pid = fork();
    if (pid < 0) {
        (void)fprintf(stderr, "[pandoc] fork() esuat: %s\n", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            (void)dup2(devnull, STDERR_FILENO);
            (void)close(devnull);
        }

        char *argv[] = {
            (char*)"pandoc",
            (char*)"-f", (char*)fromFormat,
            (char*)"-t", (char*)toFormat,
            (char*)"-o", (char*)outputPath,
            (char*)inputPath,
            NULL
        };
        execvp("pandoc", argv);

        _exit(127);
    }

    (void)fprintf(stderr, "[pandoc] Conversie pornita: %s -> %s (format: %s -> %s, pid=%d)\n",
                  inputPath, outputPath, fromFormat, toFormat, pid);

    int wstatus;
    int waitResult = waitWithTimeout(pid, timeoutSec, &wstatus);
    sigprocmask(SIG_SETMASK, &oldMask, NULL);

    if (waitResult == -1) {
        if (access(outputPath, R_OK) == 0) {
            (void)fprintf(stderr, "[pandoc] Conversie OK (post-timeout): %s\n", outputPath);
            return 0;
        }
        (void)fprintf(stderr, "[pandoc] Conversie omorata (timeout)\n");
        return -1;
    }
    if (waitResult == -2) {
        (void)fprintf(stderr, "[pandoc] waitpid esuat: %s\n", strerror(errno));
        return -1;
    }

    if (!WIFEXITED(wstatus)) {
        (void)fprintf(stderr, "[pandoc] Copilul nu a terminat normal\n");
        return -1;
    }
    int exitCode = WEXITSTATUS(wstatus);
    if (exitCode != 0) {
        (void)fprintf(stderr, "[pandoc] Pandoc a returnat exit code %d\n", exitCode);
        return -1;
    }

    if (access(outputPath, R_OK) != 0) {
        (void)fprintf(stderr, "[pandoc] Output '%s' nu a fost creat\n", outputPath);
        return -1;
    }

    (void)fprintf(stderr, "[pandoc] Conversie OK: %s\n", outputPath);
    return 0;
}
