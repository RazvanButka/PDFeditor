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


/**
 * Functie interna helper: fork + execvp + wait cu timeout.
 * Folosita de pandoc_convert si pandoc_convert_with_watermark.
 *
 * @param argv         Argumentele complete pentru execvp (terminat cu NULL)
 * @param outputPath   Calea fisierului de iesire (pentru verificare post-timeout)
 * @param timeoutSec   Timeout in secunde
 * @return 0 succes, -1 esec
 */
static int runPandocProcess(char *const argv[], const char *outputPath, int timeoutSec)
{
    sigset_t blockMask, oldMask;
    sigemptyset(&blockMask);
    sigaddset(&blockMask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &blockMask, &oldMask);

    pid_t pid = fork();
    if (pid < 0) {
        (void)fprintf(stderr, "[pandoc] fork() esuat: %s\n", strerror(errno));
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    if (pid == 0) {
        /* Copil: redirect stderr la /dev/null */
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            (void)dup2(devnull, STDERR_FILENO);
            (void)close(devnull);
        }
        execvp("pandoc", argv);
        _exit(127);
    }

    /* Logging la apel - construim string-ul pentru afisare */
    (void)fprintf(stderr, "[pandoc] Conversie pornita: pid=%d, output=%s\n",
                  pid, outputPath);

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
        case 6:  /* FMT_PDF */
            *pandocFmt = "pdf";
            *extension = "pdf";
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
    if (inputPath == NULL || outputPath == NULL ||
        fromFormat == NULL || toFormat == NULL) {
        (void)fprintf(stderr, "[pandoc] Parametru NULL\n");
        return -1;
    }
    if (timeoutSec <= 0 || timeoutSec > 600) {
        (void)fprintf(stderr, "[pandoc] Timeout %d invalid (1-600 sec)\n", timeoutSec);
        return -1;
    }
    if (access(inputPath, R_OK) != 0) {
        (void)fprintf(stderr, "[pandoc] Input '%s' indisponibil: %s\n",
                      inputPath, strerror(errno));
        return -1;
    }

    char *argv[] = {
        (char*)"pandoc",
        (char*)"-f", (char*)fromFormat,
        (char*)"-t", (char*)toFormat,
        (char*)"-o", (char*)outputPath,
        (char*)inputPath,
        NULL
    };

    return runPandocProcess(argv, outputPath, timeoutSec);
}


/**
 * Conversie cu watermark vizual pe PDF.
 *
 * Pentru output PDF, genereaza un fisier header LaTeX temporar care
 * include pachetul draftwatermark configurat cu textul dorit.
 *
 * Daca toFormat != "pdf" sau watermarkText e NULL/empty, deleaga la
 * pandoc_convert normal (fara watermark).
 *
 * @param watermarkText  Textul afisat ca watermark (max 200 caractere)
 *                       NULL sau empty => fara watermark (deleg la pandoc_convert)
 */
int pandoc_convert_with_watermark(const char *inputPath,
                                   const char *outputPath,
                                   const char *fromFormat,
                                   const char *toFormat,
                                   const char *watermarkText,
                                   int timeoutSec)
{
    if (inputPath == NULL || outputPath == NULL ||
        fromFormat == NULL || toFormat == NULL) {
        (void)fprintf(stderr, "[pandoc] Parametru NULL\n");
        return -1;
    }

    /* Fara watermark sau format != pdf => folosesc varianta simpla */
    if (watermarkText == NULL || watermarkText[0] == '\0' ||
        strcmp(toFormat, "pdf") != 0) {
        return pandoc_convert(inputPath, outputPath, fromFormat, toFormat, timeoutSec);
    }

    if (timeoutSec <= 0 || timeoutSec > 600) {
        (void)fprintf(stderr, "[pandoc] Timeout %d invalid\n", timeoutSec);
        return -1;
    }
    if (access(inputPath, R_OK) != 0) {
        (void)fprintf(stderr, "[pandoc] Input '%s' indisponibil\n", inputPath);
        return -1;
    }

    /* Creez fisier header LaTeX temporar unic */
    char headerPath[256];
    (void)snprintf(headerPath, sizeof(headerPath),
                   "/tmp/_pandoc_wm_%d.tex", (int)getpid());

    FILE *header = fopen(headerPath, "w");
    if (header == NULL) {
        (void)fprintf(stderr, "[pandoc] Nu pot crea header LaTeX: %s\n", headerPath);
        return -1;
    }

    /* Sanitizare watermark text - elimina caractere LaTeX-unsafe.
     * draftwatermark e relativ permisiv, dar evitam backslash, brace-uri si
     * specialele LaTeX care ar rupe compilarea. */
    char safeWatermark[200];
    size_t j = 0;
    for (size_t i = 0;
         watermarkText[i] != '\0' && j < sizeof(safeWatermark) - 1;
         i++) {
        char c = watermarkText[i];
        if (c == '\\' || c == '{' || c == '}' || c == '%' || c == '$' ||
            c == '#' || c == '&' || c == '~' || c == '^' || c == '_') {
            safeWatermark[j++] = ' ';
        } else {
            safeWatermark[j++] = c;
        }
    }
    safeWatermark[j] = '\0';

    (void)fprintf(header,
        "\\usepackage{draftwatermark}\n"
        "\\SetWatermarkText{%s}\n"
        "\\SetWatermarkScale{0.5}\n"
        "\\SetWatermarkColor[gray]{0.85}\n",
        safeWatermark);
    fclose(header);

    (void)fprintf(stderr, "[pandoc] Watermark header creat: %s (text=\"%s\")\n",
                  headerPath, safeWatermark);

    /* Argv pentru pandoc cu -H header.tex si --pdf-engine=pdflatex */
    char *argv[] = {
        (char*)"pandoc",
        (char*)"-f", (char*)fromFormat,
        (char*)"-t", (char*)toFormat,
        (char*)"-H", (char*)headerPath,
        (char*)"--pdf-engine=pdflatex",
        (char*)"-o", (char*)outputPath,
        (char*)inputPath,
        NULL
    };

    int rc = runPandocProcess(argv, outputPath, timeoutSec);

    /* Cleanup fisier header */
    (void)unlink(headerPath);

    return rc;
}