/**
* Burbea Alexandru, Butka Razvan */
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mupdf/fitz.h>
#include <libconfig.h>
#include <time.h>

#include "proto.h"
#include "pdf_ocr.h"
#include "pdf_server.h"
#include "gdpr_redact.h"

static const char *g_configPathOverride = NULL;

void pdf_server_set_config_path(const char *path)
{
    g_configPathOverride = path;
}

#define DEFAULT_PORT 18083
#define DEFAULT_UNIX_PATH "/tmp/admin.sock"
#define DEFAULT_INCOMING_DIR "data/input"
#define DEFAULT_OUTGOING_DIR "data/output"
#define DEFAULT_CONFIG_PATH "config/server.cfg"
#define DEFAULT_CHUNK_SIZE CHUNK_SIZE
#define DEFAULT_ADMIN_TIMEOUT 120
#define MAX_POLL_FDS 256
#define MAX_JOBS 1024
#define MAX_IP_BLACKLIST 64
#define MAX_JOB_HISTORY 512

typedef enum
{
    jobPending = 0,
    jobProcessing = 1,
    jobDone = 2,
    jobFailed = 3,
    jobKilled = 4,
} JobState;

typedef struct
{
    uint32_t jobId;
    uint32_t clientId;
    int clientSock;
    char inPath[512];
    char outPath[512];
    uint32_t operation;
    uint32_t outFmt;
    char opParam[256];
    JobState state;
    pid_t workerPid;
    time_t startTime;
    time_t endTime;
    int progress;
} JobEntry;

typedef struct
{
    uint32_t jobId;
    uint32_t clientId;
    uint32_t operation;
    JobState finalState;
    time_t startTime;
    time_t endTime;
    char inPath[256];
} JobHistoryEntry;

typedef struct
{
    int sock;
    uint32_t clientId;
    char ipAddr[46];
    uint16_t port;
    time_t connectTime;
    time_t lastActivity;
    int isAdmin;
} ClientEntry;

typedef struct
{
    int port;
    char unixPath[256];
    char incomingDir[256];
    char outgoingDir[256];
    int adminTimeout;
    int chunkSize;
} ServerConfig;

static volatile sig_atomic_t gRunning = 1;

static JobEntry gJobs[MAX_JOBS];
static int gJobCount = 0;
static uint32_t gNextJobId = 1;

static ClientEntry gClients[MAX_POLL_FDS];
static int gClientCount = 0;
static uint32_t gNextClientId = 1;

static char gIpBlacklist[MAX_IP_BLACKLIST][46];
static int gBlacklistCount = 0;

static int gAdminClientFd = -1;
static time_t gAdminLastActivity = 0;

static fz_context *gPdfCtx = NULL;

static ServerConfig gConfig;

static time_t gServerStartTime = 0;
static uint64_t gTotalJobsProcessed = 0;
static double gTotalExecSeconds = 0.0;

static JobHistoryEntry gJobHistory[MAX_JOB_HISTORY];
static int gJobHistoryCount = 0;

static int gJobQueue[MAX_JOBS];
static int gQueueHead = 0;
static int gQueueTail = 0;
static int gQueueCount = 0;
static pthread_mutex_t gQueueMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gQueueCond = PTHREAD_COND_INITIALIZER;
static pthread_t gWorkerThread;
static int gWorkerStarted = 0;

static void handleSigchld(int sig)
{
    (void)sig;
    int savedErrno = errno;
    pid_t pid;
    int wstatus;

    while ((pid = waitpid(-1, &wstatus, WNOHANG)) > 0)
    {
        for (int i = 0; i < gJobCount; i++)
        {
            if (gJobs[i].workerPid == pid)
            {
                gJobs[i].workerPid = -1;
                if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0)
                {
                    gJobs[i].state = jobDone;
                    gJobs[i].progress = 100;
                }
                else
                {
                    gJobs[i].state = jobFailed;
                }
                gJobs[i].endTime = time(NULL);
                gTotalJobsProcessed++;
                gTotalExecSeconds +=
                    difftime(gJobs[i].endTime, gJobs[i].startTime);
                if (gJobHistoryCount < MAX_JOB_HISTORY)
                {
                    JobHistoryEntry *h =
                        &gJobHistory[gJobHistoryCount++];
                    h->jobId = gJobs[i].jobId;
                    h->clientId = gJobs[i].clientId;
                    h->operation = gJobs[i].operation;
                    h->finalState = gJobs[i].state;
                    h->startTime = gJobs[i].startTime;
                    h->endTime = gJobs[i].endTime;
                    (void)strncpy(h->inPath, gJobs[i].inPath,
                                  sizeof(h->inPath) - 1);
                }
                break;
            }
        }
    }
    errno = savedErrno;
}

static void handleSigterm(int sig)
{
    (void)sig;
    gRunning = 0;
    pthread_mutex_lock(&gQueueMutex);
    pthread_cond_broadcast(&gQueueCond);
    pthread_mutex_unlock(&gQueueMutex);
}

static int dispatchJob(int jobIdx);

static const char *jobStateName(JobState st)
{
    switch (st)
    {
    case jobPending:
        return "pending";
    case jobProcessing:
        return "processing";
    case jobDone:
        return "done";
    case jobFailed:
        return "failed";
    case jobKilled:
        return "killed";
    default:
        return "unknown";
    }
}

static void enqueueJob(int jobIdx)
{
    pthread_mutex_lock(&gQueueMutex);
    if (gQueueCount < MAX_JOBS)
    {
        gJobQueue[gQueueTail] = jobIdx;
        gQueueTail = (gQueueTail + 1) % MAX_JOBS;
        gQueueCount++;
        (void)fprintf(stderr,
                      "[queue] Job %u adaugat in coada (lungime=%d)\n",
                      gJobs[jobIdx].jobId, gQueueCount);
        pthread_cond_signal(&gQueueCond);
    }
    else
    {
        (void)fprintf(stderr, "[queue] Coada plina — job %u respins\n",
                      gJobs[jobIdx].jobId);
        gJobs[jobIdx].state = jobFailed;
    }
    pthread_mutex_unlock(&gQueueMutex);
}

static void *workerThreadMain(void *arg)
{
    (void)arg;
    (void)fprintf(stderr, "[queue] Fir worker pornit (FIFO)\n");

    while (gRunning)
    {
        int jobIdx = -1;

        pthread_mutex_lock(&gQueueMutex);
        while (gQueueCount == 0 && gRunning)
        {
            pthread_cond_wait(&gQueueCond, &gQueueMutex);
        }
        if (gQueueCount > 0)
        {
            jobIdx = gJobQueue[gQueueHead];
            gQueueHead = (gQueueHead + 1) % MAX_JOBS;
            gQueueCount--;
        }
        pthread_mutex_unlock(&gQueueMutex);

        if (jobIdx >= 0)
        {
            (void)dispatchJob(jobIdx);
        }
    }

    (void)fprintf(stderr, "[queue] Fir worker oprit\n");
    return NULL;
}

static int startWorkerThread(void)
{
    if (gWorkerStarted)
    {
        return 0;
    }
    if (pthread_create(&gWorkerThread, NULL, workerThreadMain, NULL) != 0)
    {
        perror("pthread_create worker");
        return -1;
    }
    gWorkerStarted = 1;
    return 0;
}

static void configSetDefaults(void)
{
    gConfig.port = DEFAULT_PORT;
    gConfig.adminTimeout = DEFAULT_ADMIN_TIMEOUT;
    gConfig.chunkSize = DEFAULT_CHUNK_SIZE;
    (void)strncpy(gConfig.unixPath, DEFAULT_UNIX_PATH,
                  sizeof(gConfig.unixPath) - 1);
    (void)strncpy(gConfig.incomingDir, DEFAULT_INCOMING_DIR,
                  sizeof(gConfig.incomingDir) - 1);
    (void)strncpy(gConfig.outgoingDir, DEFAULT_OUTGOING_DIR,
                  sizeof(gConfig.outgoingDir) - 1);
}

static const char *resolveConfigPath(const char *cfgPath)
{
    if (cfgPath != NULL)
    {
        return cfgPath;
    }
    if (g_configPathOverride != NULL)
    {
        return g_configPathOverride;
    }
    {
        const char *env = getenv("PDF_CONFIG");
        if (env != NULL && env[0] != '\0')
        {
            return env;
        }
    }
    {
        struct stat st;
        if (stat(DEFAULT_CONFIG_PATH, &st) == 0)
        {
            return DEFAULT_CONFIG_PATH;
        }
    }
    return NULL;
}

static int loadConfig(const char *cfgPath)
{
    const char *resolved = resolveConfigPath(cfgPath);

    configSetDefaults();

    if (resolved == NULL)
    {
        return 0;
    }

    cfgPath = resolved;

    config_t cfg;
    config_init(&cfg);

    if (config_read_file(&cfg, cfgPath) == CONFIG_FALSE)
    {
        (void)fprintf(stderr, "libconfig: %s:%d — %s\n",
                      config_error_file(&cfg),
                      config_error_line(&cfg),
                      config_error_text(&cfg));
        config_destroy(&cfg);
        return -1;
    }

    int ival;
    const char *sval;

    if (config_lookup_int(&cfg, "server.port", &ival) == CONFIG_TRUE)
    {
        gConfig.port = ival;
    }
    if (config_lookup_int(&cfg, "server.admin_timeout", &ival) == CONFIG_TRUE)
    {
        gConfig.adminTimeout = ival;
    }
    if (config_lookup_int(&cfg, "server.chunk_size", &ival) == CONFIG_TRUE)
    {
        gConfig.chunkSize = ival;
    }
    if (config_lookup_string(&cfg, "server.unix_socket_path", &sval) == CONFIG_TRUE)
    {
        (void)strncpy(gConfig.unixPath, sval,
                      sizeof(gConfig.unixPath) - 1);
    }
    if (config_lookup_string(&cfg, "server.incoming_dir", &sval) == CONFIG_TRUE)
    {
        (void)strncpy(gConfig.incomingDir, sval,
                      sizeof(gConfig.incomingDir) - 1);
    }
    if (config_lookup_string(&cfg, "server.outgoing_dir", &sval) == CONFIG_TRUE)
    {
        (void)strncpy(gConfig.outgoingDir, sval,
                      sizeof(gConfig.outgoingDir) - 1);
    }

    config_destroy(&cfg);
    (void)fprintf(stderr, "loadConfig: Configuratie incarcata din %s\n",
                  cfgPath);
    return 0;
}

static void ensureDir(const char *path)
{
    struct stat st;
    if (stat(path, &st) < 0)
    {
        if (mkdir(path, 0755) < 0 && errno != EEXIST)
        {
            (void)fprintf(stderr, "mkdir(%s): %s\n", path, strerror(errno));
        }
    }
}

static int findJob(uint32_t jobId)
{
    for (int i = 0; i < gJobCount; i++)
    {
        if (gJobs[i].jobId == jobId)
        {
            return i;
        }
    }
    return -1;
}

static int findClientById(uint32_t clientId)
{
    for (int i = 0; i < gClientCount; i++)
    {
        if (gClients[i].clientId == clientId)
        {
            return i;
        }
    }
    return -1;
}

static int isIpBlocked(const char *ip)
{
    for (int i = 0; i < gBlacklistCount; i++)
    {
        if (strcmp(gIpBlacklist[i], ip) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static uint32_t allocClientId(void)
{
    return gNextClientId++;
}

static uint32_t allocJobId(void)
{
    return gNextJobId++;
}

static ClientEntry *registerClient(int sock, const char *ip,
                                   uint16_t port, int isAdmin)
{
    if (gClientCount >= MAX_POLL_FDS)
    {
        return NULL;
    }
    ClientEntry *c = &gClients[gClientCount++];
    c->sock = sock;
    c->clientId = allocClientId();
    c->port = port;
    c->isAdmin = isAdmin;
    c->connectTime = time(NULL);
    c->lastActivity = c->connectTime;
    (void)strncpy(c->ipAddr, ip, sizeof(c->ipAddr) - 1);
    return c;
}

static void removeClient(int sock)
{
    for (int i = 0; i < gClientCount; i++)
    {
        if (gClients[i].sock == sock)
        {
            (void)close(sock);
            gClients[i] = gClients[--gClientCount];
            return;
        }
    }
}

static int sendStatusResponse(int sock, uint32_t clientId,
                              uint32_t opId, uint32_t statusCode)
{
    messageHeaderType hdr;
    (void)memset(&hdr, 0, sizeof(hdr));
    hdr.messageSize = htonl((uint32_t)sizeof(hdr));
    hdr.clientID = htonl(clientId);
    hdr.opID = htonl(opId);
    hdr.statusCode = htonl(statusCode);
    return send_all(sock, &hdr, sizeof(hdr));
}

static int sendHello(int sock, uint32_t clientId)
{
    messageHeaderType hdr;
    (void)memset(&hdr, 0, sizeof(hdr));
    hdr.messageSize = htonl((uint32_t)sizeof(hdr));
    hdr.clientID = htonl(clientId);
    hdr.opID = htonl(OPR_BYE);
    hdr.statusCode = htonl(STATUS_OK);
    return send_all(sock, &hdr, sizeof(hdr));
}

static void redactSensitiveInPlace(char *text)
{
    gdpr_redact_in_place(text);
}

static char *extractAllPageText(fz_context *ctx, fz_document *doc, int pageCount)
{
    size_t capacity = 8192;
    size_t totalSize = 0;
    char *resultText = malloc(capacity);
    if (resultText == NULL)
    {
        return NULL;
    }
    resultText[0] = '\0';

    for (int pg = 0; pg < pageCount; pg++)
    {
        fz_page *page = NULL;
        fz_try(ctx)
        {
            page = fz_load_page(ctx, doc, pg);
        }
        fz_catch(ctx)
        {
            continue;
        }

        fz_buffer *buf = NULL;
        fz_output *output = NULL;
        fz_stext_page *stp = NULL;
        fz_device *dev = NULL;
        fz_try(ctx)
        {
            fz_rect mediabox = fz_bound_page(ctx, page);
            stp = fz_new_stext_page(ctx, mediabox);
            fz_stext_options stextOpts = {0};
            dev = fz_new_stext_device(ctx, stp, &stextOpts);
            fz_run_page(ctx, page, dev, fz_identity, NULL);
            fz_close_device(ctx, dev);
            fz_drop_device(ctx, dev);
            dev = NULL;

            buf = fz_new_buffer(ctx, 4096);
            output = fz_new_output_with_buffer(ctx, buf);
            fz_print_stext_page_as_text(ctx, output, stp);
            fz_close_output(ctx, output);
            fz_drop_output(ctx, output);
            output = NULL;
            fz_drop_stext_page(ctx, stp);
            stp = NULL;
        }
        fz_catch(ctx)
        {
            if (dev != NULL)
            {
                fz_drop_device(ctx, dev);
            }
            if (output != NULL)
            {
                fz_drop_output(ctx, output);
            }
            if (stp != NULL)
            {
                fz_drop_stext_page(ctx, stp);
            }
            if (buf != NULL)
            {
                fz_drop_buffer(ctx, buf);
            }
        }

        if (buf != NULL)
        {
            unsigned char *data = NULL;
            size_t dataLen = 0;
            fz_try(ctx)
            {
                dataLen = fz_buffer_storage(ctx, buf, &data);
            }
            fz_catch(ctx)
            {
                dataLen = 0;
            }

            size_t needed = totalSize + dataLen + 64;
            while (needed >= capacity)
            {
                capacity *= 2;
                char *tmp = realloc(resultText, capacity);
                if (tmp == NULL)
                {
                    free(resultText);
                    fz_drop_buffer(ctx, buf);
                    fz_drop_page(ctx, page);
                    return NULL;
                }
                resultText = tmp;
            }
            if (dataLen > 0)
            {
                int hlen = snprintf(resultText + totalSize,
                                    capacity - totalSize,
                                    "\n=== Page %d ===\n", pg + 1);
                if (hlen > 0)
                {
                    totalSize += (size_t)hlen;
                }
                (void)memcpy(resultText + totalSize, data, dataLen);
                totalSize += dataLen;
                resultText[totalSize] = '\0';
            }
            fz_drop_buffer(ctx, buf);
        }
        fz_drop_page(ctx, page);
    }
    return resultText;
}

static void executeJobChild(const JobEntry *job)
{
    fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    if (ctx == NULL)
    {
        (void)fprintf(stderr,
                      "[worker %u] fz_new_context esuat\n", job->jobId);
        _exit(EXIT_FAILURE);
    }
    fz_try(ctx)
    {
        fz_register_document_handlers(ctx);
    }
    fz_catch(ctx)
    {
        (void)fprintf(stderr,
                      "[worker %u] fz_register_document_handlers esuat\n",
                      job->jobId);
        fz_drop_context(ctx);
        _exit(EXIT_FAILURE);
    }

    fz_document *doc = NULL;
    fz_try(ctx)
    {
        doc = fz_open_document(ctx, job->inPath);
    }
    fz_catch(ctx)
    {
        (void)fprintf(stderr,
                      "[worker %u] nu pot deschide %s\n",
                      job->jobId, job->inPath);
        fz_drop_context(ctx);
        _exit(EXIT_FAILURE);
    }

    int pageCount = 0;
    fz_try(ctx)
    {
        pageCount = fz_count_pages(ctx, doc);
    }
    fz_catch(ctx)
    {
        pageCount = 0;
    }

    char *resultText = NULL;

    switch (job->operation)
    {
    case OPR_PDF_OCR:
    case OPR_PDF_OCR_PAGE:
        resultText = ocr_extract_all_text_parallel(
            job->inPath, 300, pageCount);
        break;

    case OPR_PDF_EXTRACT_TEXT:
    case OPR_PDF_CONVERT:
        resultText = extractAllPageText(ctx, doc, pageCount);
        break;

    case OPR_PDF_WATERMARK:
    {
        resultText = extractAllPageText(ctx, doc, pageCount);
        if (resultText != NULL)
        {
            const char *wm =
                (job->opParam[0] != '\0') ? job->opParam : "CONFIDENTIAL";
            size_t wlen = strlen(wm);
            size_t tlen = strlen(resultText);
            char *combined = malloc(wlen + tlen + 64);
            if (combined != NULL)
            {
                (void)snprintf(combined, wlen + tlen + 64,
                               "=== WATERMARK: %s ===\n\n%s", wm, resultText);
                free(resultText);
                resultText = combined;
            }
        }
        break;
    }

    case OPR_PDF_BLUR_GDPR:
        resultText = extractAllPageText(ctx, doc, pageCount);
        if (resultText != NULL)
        {
            redactSensitiveInPlace(resultText);
        }
        break;

    default:
        (void)fprintf(stderr,
                      "[worker %u] Operatie necunoscuta: %u\n",
                      job->jobId, job->operation);
        break;
    }

    fz_drop_document(ctx, doc);
    fz_drop_context(ctx);

    if (resultText != NULL)
    {
        int outFd = open(job->outPath,
                         O_WRONLY | O_CREAT | O_TRUNC,
                         S_IRUSR | S_IWUSR | S_IRGRP);
        if (outFd >= 0)
        {
            size_t len = strlen(resultText);
            ssize_t wr = write(outFd, resultText, len);
            (void)close(outFd);
            if (wr < 0 || (size_t)wr != len)
            {
                (void)fprintf(stderr,
                              "[worker %u] write rezultat esuat\n",
                              job->jobId);
                free(resultText);
                _exit(EXIT_FAILURE);
            }
        }
        else
        {
            (void)fprintf(stderr,
                          "[worker %u] open(%s) esuat: %s\n",
                          job->jobId, job->outPath, strerror(errno));
            free(resultText);
            _exit(EXIT_FAILURE);
        }
        free(resultText);
    }

    _exit(EXIT_SUCCESS);
}

static int dispatchJob(int jobIdx)
{
    JobEntry *job = &gJobs[jobIdx];

    (void)snprintf(job->outPath, sizeof(job->outPath),
                   "%s/result_%u.txt",
                   gConfig.outgoingDir, job->jobId);

    job->state = jobProcessing;
    job->startTime = time(NULL);
    job->progress = 0;

    pid_t pid = fork();
    if (pid < 0)
    {
        (void)fprintf(stderr, "dispatchJob: fork() esuat: %s\n",
                      strerror(errno));
        job->state = jobFailed;
        return -1;
    }

    if (pid == 0)
    {
        executeJobChild(job);
    }

    job->workerPid = pid;
    (void)fprintf(stderr,
                  "[server] Job %u dispatched → PID %d\n",
                  job->jobId, pid);
    return 0;
}

static uint32_t handleUploadStart(int sock, uint32_t clientId,
                                  const uploadStartmessageType *msg)
{
    if (gJobCount >= MAX_JOBS)
    {
        (void)sendStatusResponse(sock, clientId,
                                 OPR_UPLOAD_START, STATUS_ERROR);
        return 0;
    }

    uint32_t jobId = allocJobId();
    JobEntry *job = &gJobs[gJobCount++];
    (void)memset(job, 0, sizeof(*job));

    job->jobId = jobId;
    job->clientId = clientId;
    job->clientSock = sock;
    job->operation = ntohl(msg->opType);
    job->outFmt = ntohl(msg->outFmt);
    (void)strncpy(job->opParam, msg->opParam, sizeof(job->opParam) - 1);
    job->opParam[sizeof(job->opParam) - 1] = '\0';
    job->state = jobPending;
    job->workerPid = -1;

    char safeName[256];
    (void)strncpy(safeName, msg->fileName, sizeof(safeName) - 1);
    for (char *p = safeName; *p != '\0'; p++)
    {
        if (*p == '/' || *p == '\\' || *p == '.')
        {
            *p = '_';
        }
    }
    (void)snprintf(job->inPath, sizeof(job->inPath),
                   "%s/%u_%s", gConfig.incomingDir, jobId, safeName);

    uploadAckmessageType ack;
    (void)memset(&ack, 0, sizeof(ack));
    ack.header.messageSize = htonl((uint32_t)sizeof(ack));
    ack.header.clientID = htonl(clientId);
    ack.header.opID = htonl(OPR_UPLOAD_START);
    ack.header.statusCode = htonl(STATUS_UPLOAD_READY);
    ack.jobID = htonl(jobId);

    if (send_all(sock, &ack, sizeof(ack)) < 0)
    {
        return 0;
    }
    (void)fprintf(stderr,
                  "[server] Job %u creat pentru client %u → %s\n",
                  jobId, clientId, job->inPath);
    return jobId;
}

static int handleUploadChunk(int sock, uint32_t clientId,
                             uint32_t jobId)
{
    uint32_t chunkLenNet;
    if (recv_all(sock, &chunkLenNet, sizeof(chunkLenNet)) < 0)
    {
        return -1;
    }
    uint32_t chunkLen = ntohl(chunkLenNet);
    if (chunkLen == 0 || chunkLen > (uint32_t)CHUNK_SIZE)
    {
        (void)fprintf(stderr,
                      "[server] Dimensiune bloc invalida: %u\n", chunkLen);
        return -1;
    }

    uint8_t data[CHUNK_SIZE];
    if (recv_all(sock, data, chunkLen) < 0)
    {
        return -1;
    }

    int jobIdx = findJob(jobId);
    if (jobIdx < 0)
    {
        (void)fprintf(stderr,
                      "[server] UPLOAD_CHUNK: job %u negasit\n", jobId);
        return -1;
    }

    int fd = open(gJobs[jobIdx].inPath,
                  O_WRONLY | O_CREAT | O_APPEND,
                  S_IRUSR | S_IWUSR);
    if (fd < 0)
    {
        (void)fprintf(stderr, "open(%s): %s\n",
                      gJobs[jobIdx].inPath, strerror(errno));
        return -1;
    }
    ssize_t wr = write(fd, data, chunkLen);
    (void)close(fd);

    if (wr < 0 || (size_t)wr != chunkLen)
    {
        return -1;
    }
    (void)(clientId);
    return 0;
}

static int handleUploadEnd(int sock, uint32_t clientId, uint32_t jobId)
{
    int jobIdx = findJob(jobId);
    if (jobIdx < 0)
    {
        (void)sendStatusResponse(sock, clientId,
                                 OPR_UPLOAD_END, STATUS_NOT_FOUND);
        return -1;
    }

    enqueueJob(jobIdx);

    uploadAckmessageType ack;
    (void)memset(&ack, 0, sizeof(ack));
    ack.header.messageSize = htonl((uint32_t)sizeof(ack));
    ack.header.clientID = htonl(clientId);
    ack.header.opID = htonl(OPR_UPLOAD_END);
    ack.header.statusCode = htonl(STATUS_UPLOAD_COMPLETE);
    ack.jobID = htonl(jobId);

    return send_all(sock, &ack, sizeof(ack));
}

static void handleStatusReq(int sock, uint32_t clientId, uint32_t jobId)
{
    jobStatusRespType resp;
    (void)memset(&resp, 0, sizeof(resp));
    resp.header.messageSize = htonl((uint32_t)sizeof(resp));
    resp.header.clientID = htonl(clientId);
    resp.header.opID = htonl(OPR_STATUS_REQ);
    resp.header.statusCode = htonl(STATUS_OK);
    resp.jobID = htonl(jobId);

    int jobIdx = findJob(jobId);
    if (jobIdx < 0)
    {
        resp.jobStatus = htonl((uint32_t)STATUS_NOT_FOUND);
        resp.progress = 0;
    }
    else
    {
        JobEntry *j = &gJobs[jobIdx];
        uint32_t js;
        switch (j->state)
        {
        case jobPending:
            js = STATUS_JOB_PENDING;
            break;
        case jobProcessing:
            js = STATUS_JOB_PROCESSING;
            break;
        case jobDone:
            js = STATUS_JOB_DONE;
            break;
        case jobFailed:
            js = STATUS_JOB_FAILED;
            break;
        case jobKilled:
            js = STATUS_JOB_KILLED;
            break;
        default:
            js = STATUS_ERROR;
            break;
        }
        resp.jobStatus = htonl(js);
        resp.progress = htonl((uint32_t)j->progress);
    }

    (void)send_all(sock, &resp, sizeof(resp));
}

static void handleDownloadReq(int sock, uint32_t clientId,
                              uint32_t jobId, uint32_t outFmt)
{
    int jobIdx = findJob(jobId);
    if (jobIdx < 0)
    {
        (void)sendStatusResponse(sock, clientId,
                                 OPR_DOWNLOAD_REQ, STATUS_NOT_FOUND);
        return;
    }

    JobEntry *job = &gJobs[jobIdx];
    if (job->state == jobPending || job->state == jobProcessing)
    {
        (void)sendStatusResponse(sock, clientId,
                                 OPR_DOWNLOAD_REQ, STATUS_JOB_PROCESSING);
        return;
    }
    if (job->state != jobDone)
    {
        (void)sendStatusResponse(sock, clientId,
                                 OPR_DOWNLOAD_REQ, STATUS_JOB_FAILED);
        return;
    }

    char finalPath[512];
    (void)strncpy(finalPath, job->outPath, sizeof(finalPath) - 1);

    if ((OutputFormat)outFmt != FMT_KEEP && (OutputFormat)outFmt != FMT_TXT)
    {
        const char *ext;
        switch ((OutputFormat)outFmt)
        {
        case FMT_DOCX:
            ext = "docx";
            break;
        case FMT_HTML:
            ext = "html";
            break;
        case FMT_MD:
            ext = "md";
            break;
        case FMT_RTF:
            ext = "rtf";
            break;
        default:
            ext = "txt";
            break;
        }
        char convertedPath[512];
        (void)snprintf(convertedPath, sizeof(convertedPath),
                       "%s/result_%u.%s",
                       gConfig.outgoingDir, jobId, ext);

        char cmd[1024];
        (void)snprintf(cmd, sizeof(cmd),
                       "pandoc -f plain -t %s -o \"%s\" \"%s\" 2>/dev/null",
                       ext, convertedPath, job->outPath);
        if (system(cmd) == 0)
        {
            (void)strncpy(finalPath, convertedPath,
                          sizeof(finalPath) - 1);
        }
    }

    struct stat st;
    if (stat(finalPath, &st) < 0)
    {
        (void)sendStatusResponse(sock, clientId,
                                 OPR_DOWNLOAD_REQ, STATUS_ERROR);
        return;
    }
    uint64_t fileSize = (uint64_t)st.st_size;

    downloadAckType ack;
    (void)memset(&ack, 0, sizeof(ack));
    ack.header.messageSize = htonl((uint32_t)sizeof(ack));
    ack.header.clientID = htonl(clientId);
    ack.header.opID = htonl(OPR_DOWNLOAD_REQ);
    ack.header.statusCode = htonl(STATUS_DOWNLOAD_READY);
    ack.fileSizeHigh = htonl(FSIZE_HIGH(fileSize));
    ack.fileSizeLow = htonl(FSIZE_LOW(fileSize));

    (void)snprintf(ack.fileName, sizeof(ack.fileName),
                   "result_%u", jobId);

    if (send_all(sock, &ack, sizeof(ack)) < 0)
    {
        return;
    }

    int fd = open(finalPath, O_RDONLY);
    if (fd < 0)
    {
        (void)fprintf(stderr, "download open(%s): %s\n",
                      finalPath, strerror(errno));
        return;
    }

    uploadChunkmessageType chunk;
    ssize_t bytesRead;

    while ((bytesRead = read(fd, chunk.data, (size_t)gConfig.chunkSize)) > 0)
    {
        (void)memset(&chunk.header, 0, sizeof(chunk.header));
        chunk.header.messageSize = htonl((uint32_t)(sizeof(messageHeaderType) +
                                                    sizeof(uint32_t) +
                                                    (uint32_t)bytesRead));
        chunk.header.clientID = htonl(clientId);
        chunk.header.opID = htonl(OPR_DOWNLOAD_CHUNK);
        chunk.header.statusCode = htonl(STATUS_OK);
        chunk.chunkLen = htonl((uint32_t)bytesRead);

        size_t totalSend = sizeof(messageHeaderType) + sizeof(uint32_t) +
                           (size_t)bytesRead;
        if (send_all(sock, &chunk, totalSend) < 0)
        {
            break;
        }
    }
    (void)close(fd);

    (void)sendStatusResponse(sock, clientId,
                             OPR_DOWNLOAD_END, STATUS_OK);
    (void)fprintf(stderr,
                  "[server] Download job %u complet (%" PRIu64 " bytes)\n",
                  jobId, fileSize);
}

static void handleAdminListClients(int sock, uint32_t clientId)
{
    char buf[4096];
    int len = 0;

    len += snprintf(buf + len, sizeof(buf) - (size_t)len,
                    "=== Clienti conectati: %d ===\n", gClientCount);
    for (int i = 0; i < gClientCount; i++)
    {
        ClientEntry *c = &gClients[i];
        len += snprintf(buf + len, sizeof(buf) - (size_t)len,
                        "  [%u] %s:%" PRIu16 " (admin=%d) activ acum %lds\n",
                        c->clientId, c->ipAddr, c->port, c->isAdmin,
                        (long)difftime(time(NULL), c->lastActivity));
    }

    messageHeaderType hdr;
    (void)memset(&hdr, 0, sizeof(hdr));
    uint32_t payloadLen = (uint32_t)strlen(buf);
    hdr.messageSize = htonl(sizeof(hdr) + sizeof(uint32_t) + payloadLen);
    hdr.clientID = htonl(clientId);
    hdr.opID = htonl(OPR_ADMIN_LIST_CLIENTS);
    hdr.statusCode = htonl(STATUS_OK);
    (void)send_all(sock, &hdr, sizeof(hdr));
    uint32_t plenNet = htonl(payloadLen);
    (void)send_all(sock, &plenNet, sizeof(plenNet));
    (void)send_all(sock, buf, payloadLen);
}

static void handleAdminListJobs(int sock, uint32_t clientId)
{
    char buf[8192];
    int len = 0;
    const char *stateNames[] = {
        "PENDING", "PROCESSING", "DONE", "FAILED", "KILLED"};

    len += snprintf(buf + len, sizeof(buf) - (size_t)len,
                    "=== Joburi totale: %d ===\n", gJobCount);
    for (int i = 0; i < gJobCount; i++)
    {
        JobEntry *j = &gJobs[i];
        int sidx = (int)j->state;
        if (sidx < 0 || sidx > 4)
        {
            sidx = 4;
        }
        len += snprintf(buf + len, sizeof(buf) - (size_t)len,
                        "  Job[%u] client=%u op=%u state=%s progress=%d%%\n",
                        j->jobId, j->clientId, j->operation,
                        stateNames[sidx], j->progress);
    }

    messageHeaderType hdr;
    (void)memset(&hdr, 0, sizeof(hdr));
    uint32_t payloadLen = (uint32_t)strlen(buf);
    hdr.messageSize = htonl(sizeof(hdr) + sizeof(uint32_t) + payloadLen);
    hdr.clientID = htonl(clientId);
    hdr.opID = htonl(OPR_ADMIN_LIST_JOBS);
    hdr.statusCode = htonl(STATUS_OK);
    (void)send_all(sock, &hdr, sizeof(hdr));
    uint32_t plenNet = htonl(payloadLen);
    (void)send_all(sock, &plenNet, sizeof(plenNet));
    (void)send_all(sock, buf, payloadLen);
}

static void handleAdminKillJob(int sock, uint32_t clientId,
                               uint32_t targetJobId)
{
    int jobIdx = findJob(targetJobId);
    if (jobIdx < 0)
    {
        (void)sendStatusResponse(sock, clientId,
                                 OPR_ADMIN_KILL_JOB, STATUS_NOT_FOUND);
        return;
    }
    JobEntry *j = &gJobs[jobIdx];
    if (j->workerPid > 0)
    {
        if (kill(j->workerPid, SIGTERM) < 0)
        {
            (void)fprintf(stderr,
                          "[admin] kill(%d): %s\n",
                          j->workerPid, strerror(errno));
        }
    }
    j->state = jobKilled;
    j->endTime = time(NULL);
    (void)sendStatusResponse(sock, clientId,
                             OPR_ADMIN_KILL_JOB, STATUS_OK);
    (void)fprintf(stderr,
                  "[admin] Job %u terminat fortat.\n", targetJobId);
}

static void handleAdminKickClient(int sock, uint32_t clientId,
                                  uint32_t targetClientId)
{
    int idx = findClientById(targetClientId);
    if (idx < 0)
    {
        (void)sendStatusResponse(sock, clientId,
                                 OPR_ADMIN_KICK_CLIENT, STATUS_NOT_FOUND);
        return;
    }
    if (gClients[idx].isAdmin)
    {
        (void)sendStatusResponse(sock, clientId,
                                 OPR_ADMIN_KICK_CLIENT, STATUS_UNAUTHORIZED);
        return;
    }
    int targetSock = gClients[idx].sock;
    (void)sendStatusResponse(targetSock, targetClientId,
                             OPR_BYE, STATUS_UNAUTHORIZED);
    removeClient(targetSock);
    (void)sendStatusResponse(sock, clientId,
                             OPR_ADMIN_KICK_CLIENT, STATUS_OK);
    (void)fprintf(stderr,
                  "[admin] Client %u deconectat fortat.\n", targetClientId);
}

static void handleAdminBlockIp(int sock, uint32_t clientId,
                               const char *ip, int block)
{
    if (block)
    {
        if (isIpBlocked(ip))
        {
            (void)sendStatusResponse(sock, clientId,
                                     OPR_ADMIN_BLOCK_IP,
                                     STATUS_ALREADY_BLOCKED);
            return;
        }
        if (gBlacklistCount < MAX_IP_BLACKLIST)
        {
            (void)strncpy(gIpBlacklist[gBlacklistCount++], ip, 45);
        }
        (void)sendStatusResponse(sock, clientId,
                                 OPR_ADMIN_BLOCK_IP, STATUS_OK);
    }
    else
    {
        for (int i = 0; i < gBlacklistCount; i++)
        {
            if (strcmp(gIpBlacklist[i], ip) == 0)
            {
                (void)memcpy(gIpBlacklist[i],
                             gIpBlacklist[gBlacklistCount - 1],
                             sizeof(gIpBlacklist[i]));
                gBlacklistCount--;
                (void)sendStatusResponse(sock, clientId,
                                         OPR_ADMIN_UNBLOCK_IP, STATUS_OK);
                return;
            }
        }
        (void)sendStatusResponse(sock, clientId,
                                 OPR_ADMIN_UNBLOCK_IP, STATUS_NOT_FOUND);
    }
}

static void handleAdminJobHistory(int sock, uint32_t clientId)
{
    char buf[8192];
    int len = 0;

    len += snprintf(buf + len, sizeof(buf) - (size_t)len,
                    "=== Istoric joburi (%d intrari) ===\n",
                    gJobHistoryCount);
    for (int i = 0; i < gJobHistoryCount; i++)
    {
        JobHistoryEntry *h = &gJobHistory[i];
        len += snprintf(buf + len, sizeof(buf) - (size_t)len,
                        "  [%u] client=%u op=%u state=%s dur=%lds file=%s\n",
                        h->jobId, h->clientId, h->operation,
                        jobStateName(h->finalState),
                        (long)difftime(h->endTime, h->startTime),
                        h->inPath);
        if (len >= (int)sizeof(buf) - 128)
        {
            break;
        }
    }

    messageHeaderType hdr;
    (void)memset(&hdr, 0, sizeof(hdr));
    uint32_t payloadLen = (uint32_t)strlen(buf);
    hdr.messageSize = htonl(sizeof(hdr) + sizeof(uint32_t) + payloadLen);
    hdr.clientID = htonl(clientId);
    hdr.opID = htonl(OPR_ADMIN_JOB_HISTORY);
    hdr.statusCode = htonl(STATUS_OK);
    (void)send_all(sock, &hdr, sizeof(hdr));
    uint32_t plenNet = htonl(payloadLen);
    (void)send_all(sock, &plenNet, sizeof(plenNet));
    (void)send_all(sock, buf, payloadLen);
}

static void handleClientJobList(int sock, uint32_t clientId)
{
    char buf[4096];
    int len = 0;

    len += snprintf(buf + len, sizeof(buf) - (size_t)len,
                    "=== Joburi client %u ===\n", clientId);
    for (int i = 0; i < gJobCount; i++)
    {
        if (gJobs[i].clientId != clientId)
        {
            continue;
        }
        len += snprintf(buf + len, sizeof(buf) - (size_t)len,
                        "  [%u] op=%u state=%s progress=%d%%\n",
                        gJobs[i].jobId, gJobs[i].operation,
                        jobStateName(gJobs[i].state), gJobs[i].progress);
    }

    messageHeaderType hdr;
    (void)memset(&hdr, 0, sizeof(hdr));
    uint32_t payloadLen = (uint32_t)strlen(buf);
    hdr.messageSize = htonl(sizeof(hdr) + sizeof(uint32_t) + payloadLen);
    hdr.clientID = htonl(clientId);
    hdr.opID = htonl(OPR_JOB_LIST);
    hdr.statusCode = htonl(STATUS_OK);
    (void)send_all(sock, &hdr, sizeof(hdr));
    uint32_t plenNet = htonl(payloadLen);
    (void)send_all(sock, &plenNet, sizeof(plenNet));
    (void)send_all(sock, buf, payloadLen);
}

static void handleAdminSysStats(int sock, uint32_t clientId)
{
    char buf[2048];
    double avg = (gTotalJobsProcessed > 0)
                     ? gTotalExecSeconds / (double)gTotalJobsProcessed
                     : 0.0;

    (void)snprintf(buf, sizeof(buf),
                   "=== Statistici Sistem ===\n"
                   "Uptime:          %lds\n"
                   "Clienti activi:  %d\n"
                   "Joburi totale:   %" PRIu64 "\n"
                   "IP-uri blocate:  %d\n"
                   "Durata medie:    %.2fs\n",
                   (long)difftime(time(NULL), gServerStartTime),
                   gClientCount,
                   gTotalJobsProcessed,
                   gBlacklistCount,
                   avg);

    messageHeaderType hdr;
    (void)memset(&hdr, 0, sizeof(hdr));
    uint32_t payloadLen = (uint32_t)strlen(buf);
    hdr.messageSize = htonl(sizeof(hdr) + sizeof(uint32_t) + payloadLen);
    hdr.clientID = htonl(clientId);
    hdr.opID = htonl(OPR_ADMIN_SYS_STATS);
    hdr.statusCode = htonl(STATUS_OK);
    (void)send_all(sock, &hdr, sizeof(hdr));
    uint32_t plenNet = htonl(payloadLen);
    (void)send_all(sock, &plenNet, sizeof(plenNet));
    (void)send_all(sock, buf, payloadLen);
}

static int processClientMessage(int sock, uint32_t clientId,
                                time_t *lastActivity)
{
    messageHeaderType hdr;
    if (recv_all(sock, &hdr, sizeof(hdr)) < 0)
    {
        return -1;
    }
    *lastActivity = time(NULL);

    uint32_t op = ntohl(hdr.opID);
    uint32_t msgSize = ntohl(hdr.messageSize);

    (void)fprintf(stderr,
                  "[server] Client %u → op=%u size=%u\n",
                  clientId, op, msgSize);

    switch (op)
    {
    case OPR_BYE:
        return -1;

    case OPR_UPLOAD_START:
    {
        uploadStartmessageType msg;
        (void)memset(&msg, 0, sizeof(msg));
        msg.header = hdr;
        size_t rest = sizeof(msg) - sizeof(messageHeaderType);
        if (recv_all(sock, (char *)&msg + sizeof(messageHeaderType), rest) < 0)
        {
            return -1;
        }
        (void)handleUploadStart(sock, clientId, &msg);
        break;
    }

    case OPR_UPLOAD_CHUNK:
    {
        uint32_t jobId = 0;
        for (int i = gJobCount - 1; i >= 0; i--)
        {
            if (gJobs[i].clientId == clientId &&
                gJobs[i].state == jobPending)
            {
                jobId = gJobs[i].jobId;
                break;
            }
        }
        if (jobId == 0 ||
            handleUploadChunk(sock, clientId, jobId) < 0)
        {
            return -1;
        }
        break;
    }

    case OPR_UPLOAD_END:
    {
        uint32_t jobId = 0;
        for (int i = gJobCount - 1; i >= 0; i--)
        {
            if (gJobs[i].clientId == clientId &&
                gJobs[i].state == jobPending)
            {
                jobId = gJobs[i].jobId;
                break;
            }
        }
        if (jobId == 0 ||
            handleUploadEnd(sock, clientId, jobId) < 0)
        {
            return -1;
        }
        break;
    }

    case OPR_STATUS_REQ:
    {
        jobStatusReqType req;
        req.header = hdr;
        size_t rest = sizeof(req) - sizeof(messageHeaderType);
        if (recv_all(sock, (char *)&req + sizeof(messageHeaderType), rest) < 0)
        {
            return -1;
        }
        handleStatusReq(sock, clientId, ntohl(req.jobID));
        break;
    }

    case OPR_DOWNLOAD_REQ:
    {
        downloadReqType req;
        req.header = hdr;
        size_t rest = sizeof(req) - sizeof(messageHeaderType);
        if (recv_all(sock, (char *)&req + sizeof(messageHeaderType), rest) < 0)
        {
            return -1;
        }
        handleDownloadReq(sock, clientId,
                          ntohl(req.jobID), ntohl(req.outFmt));
        break;
    }

    case OPR_JOB_LIST:
        handleClientJobList(sock, clientId);
        break;

    default:
        (void)fprintf(stderr,
                      "[server] OpCode necunoscut %u de la client %u\n",
                      op, clientId);
        break;
    }

    return 0;
}

static int processAdminMessage(int sock, uint32_t clientId)
{
    messageHeaderType hdr;
    if (recv_all(sock, &hdr, sizeof(hdr)) < 0)
    {
        return -1;
    }
    gAdminLastActivity = time(NULL);

    uint32_t op = ntohl(hdr.opID);

    switch (op)
    {
    case OPR_BYE:
        return -1;

    case OPR_ADMIN_LIST_CLIENTS:
        handleAdminListClients(sock, clientId);
        break;

    case OPR_ADMIN_LIST_JOBS:
        handleAdminListJobs(sock, clientId);
        break;

    case OPR_ADMIN_SYS_STATS:
        handleAdminSysStats(sock, clientId);
        break;

    case OPR_ADMIN_JOB_HISTORY:
        handleAdminJobHistory(sock, clientId);
        break;

    case OPR_ADMIN_AVG_EXEC_TIME:
    {
        double avg = (gTotalJobsProcessed > 0)
                         ? gTotalExecSeconds / (double)gTotalJobsProcessed
                         : 0.0;
        char buf[256];
        (void)snprintf(buf, sizeof(buf),
                       "Durata medie executie: %.2fs (%" PRIu64 " joburi)\n",
                       avg, gTotalJobsProcessed);
        messageHeaderType rhdr;
        (void)memset(&rhdr, 0, sizeof(rhdr));
        uint32_t pl = (uint32_t)strlen(buf);
        rhdr.messageSize = htonl(sizeof(rhdr) + sizeof(uint32_t) + pl);
        rhdr.clientID = htonl(clientId);
        rhdr.opID = htonl(OPR_ADMIN_AVG_EXEC_TIME);
        rhdr.statusCode = htonl(STATUS_OK);
        (void)send_all(sock, &rhdr, sizeof(rhdr));
        uint32_t plNet = htonl(pl);
        (void)send_all(sock, &plNet, sizeof(plNet));
        (void)send_all(sock, buf, pl);
        break;
    }

    case OPR_ADMIN_KILL_JOB:
    {
        adminKillJobmessageType msg;
        msg.header = hdr;
        size_t rest = sizeof(msg) - sizeof(messageHeaderType);
        if (recv_all(sock, (char *)&msg + sizeof(messageHeaderType), rest) < 0)
        {
            return -1;
        }
        handleAdminKillJob(sock, clientId, ntohl(msg.targetJobID));
        break;
    }

    case OPR_ADMIN_KICK_CLIENT:
    {
        adminKickmessageType msg;
        msg.header = hdr;
        size_t rest = sizeof(msg) - sizeof(messageHeaderType);
        if (recv_all(sock, (char *)&msg + sizeof(messageHeaderType), rest) < 0)
        {
            return -1;
        }
        handleAdminKickClient(sock, clientId, ntohl(msg.targetClientID));
        break;
    }

    case OPR_ADMIN_BLOCK_IP:
    {
        adminIPmessageType msg;
        msg.header = hdr;
        size_t rest = sizeof(msg) - sizeof(messageHeaderType);
        if (recv_all(sock, (char *)&msg + sizeof(messageHeaderType), rest) < 0)
        {
            return -1;
        }
        msg.ipAddr[sizeof(msg.ipAddr) - 1] = '\0';
        handleAdminBlockIp(sock, clientId, msg.ipAddr, 1);
        break;
    }

    case OPR_ADMIN_UNBLOCK_IP:
    {
        adminIPmessageType msg;
        msg.header = hdr;
        size_t rest = sizeof(msg) - sizeof(messageHeaderType);
        if (recv_all(sock, (char *)&msg + sizeof(messageHeaderType), rest) < 0)
        {
            return -1;
        }
        msg.ipAddr[sizeof(msg.ipAddr) - 1] = '\0';
        handleAdminBlockIp(sock, clientId, msg.ipAddr, 0);
        break;
    }

    default:
        (void)fprintf(stderr,
                      "[admin] OpCode necunoscut: %u\n", op);
        break;
    }

    return 0;
}

static int createInetSocket(int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket INET");
        return -1;
    }

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt SO_REUSEADDR");
        (void)close(sock);
        return -1;
    }

    struct sockaddr_in addr;
    (void)memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind INET");
        (void)close(sock);
        return -1;
    }

    if (listen(sock, 16) < 0)
    {
        perror("listen INET");
        (void)close(sock);
        return -1;
    }

    (void)fprintf(stderr, "[server] Ascult INET pe portul %d\n", port);
    return sock;
}

static int createUnixSocket(const char *path)
{
    (void)unlink(path);

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket UNIX");
        return -1;
    }

    struct sockaddr_un addr;
    (void)memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    (void)strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind UNIX");
        (void)close(sock);
        return -1;
    }

    if (listen(sock, 1) < 0)
    {
        perror("listen UNIX");
        (void)close(sock);
        return -1;
    }

    (void)fprintf(stderr, "[server] Ascult UNIX pe %s\n", path);
    return sock;
}

static void serverLoop(int inetSock, int unixSock)
{
    struct pollfd pfds[MAX_POLL_FDS];
    int nfds = 0;

    pfds[nfds].fd = inetSock;
    pfds[nfds].events = POLLIN;
    nfds++;

    pfds[nfds].fd = unixSock;
    pfds[nfds].events = POLLIN;
    nfds++;

    while (gRunning)
    {
        if (gAdminClientFd >= 0 && gAdminLastActivity > 0)
        {
            if (difftime(time(NULL), gAdminLastActivity) >
                (double)gConfig.adminTimeout)
            {
                (void)fprintf(stderr,
                              "[server] Admin timeout – deconectare.\n");
                (void)sendStatusResponse(gAdminClientFd, 0,
                                         OPR_BYE, STATUS_UNAUTHORIZED);
                (void)close(gAdminClientFd);
                for (int i = 2; i < nfds; i++)
                {
                    if (pfds[i].fd == gAdminClientFd)
                    {
                        pfds[i] = pfds[--nfds];
                        break;
                    }
                }
                removeClient(gAdminClientFd);
                gAdminClientFd = -1;
            }
        }

        int ready = poll(pfds, (nfds_t)nfds, 1000);
        if (ready < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("poll()");
            break;
        }

        for (int i = 0; i < nfds && ready > 0; i++)
        {
            if (!(pfds[i].revents & (POLLIN | POLLHUP | POLLERR)))
            {
                continue;
            }
            ready--;

            if (pfds[i].fd == inetSock)
            {
                struct sockaddr_in clientAddr;
                socklen_t addrLen = sizeof(clientAddr);
                int csock = accept(inetSock,
                                   (struct sockaddr *)&clientAddr,
                                   &addrLen);
                if (csock < 0)
                {
                    perror("accept INET");
                    continue;
                }

                char ip[46];
                (void)inet_ntop(AF_INET, &clientAddr.sin_addr, ip, sizeof(ip));
                uint16_t port = ntohs(clientAddr.sin_port);

                if (isIpBlocked(ip))
                {
                    (void)fprintf(stderr,
                                  "[server] IP blocat refuzat: %s\n", ip);
                    (void)close(csock);
                    continue;
                }

                ClientEntry *c = registerClient(csock, ip, port, 0);
                if (c == NULL)
                {
                    (void)close(csock);
                    continue;
                }
                (void)fprintf(stderr,
                              "[server] Client ordinar %u conectat: %s:%" PRIu16 "\n",
                              c->clientId, ip, port);

                (void)sendHello(csock, c->clientId);

                if (nfds < MAX_POLL_FDS)
                {
                    pfds[nfds].fd = csock;
                    pfds[nfds].events = POLLIN;
                    nfds++;
                }
                continue;
            }

            if (pfds[i].fd == unixSock)
            {
                struct sockaddr_un adminAddr;
                socklen_t addrLen = sizeof(adminAddr);
                int asock = accept(unixSock,
                                   (struct sockaddr *)&adminAddr,
                                   &addrLen);
                if (asock < 0)
                {
                    perror("accept UNIX");
                    continue;
                }

                if (gAdminClientFd >= 0)
                {
                    (void)fprintf(stderr,
                                  "[server] Admin deja conectat – refuz.\n");
                    (void)sendStatusResponse(asock, 0,
                                             OPR_BYE, STATUS_UNAUTHORIZED);
                    (void)close(asock);
                    continue;
                }

                ClientEntry *c = registerClient(asock, "127.0.0.1", 0, 1);
                if (c == NULL)
                {
                    (void)close(asock);
                    continue;
                }
                gAdminClientFd = asock;
                gAdminLastActivity = time(NULL);
                (void)fprintf(stderr,
                              "[server] Client admin %u conectat.\n",
                              c->clientId);
                (void)sendHello(asock, c->clientId);

                if (nfds < MAX_POLL_FDS)
                {
                    pfds[nfds].fd = asock;
                    pfds[nfds].events = POLLIN;
                    nfds++;
                }
                continue;
            }

            int csock = pfds[i].fd;
            int isAdminConn = (csock == gAdminClientFd);

            uint32_t cid = 0;
            time_t *lastActivity = NULL;
            for (int j = 0; j < gClientCount; j++)
            {
                if (gClients[j].sock == csock)
                {
                    cid = gClients[j].clientId;
                    lastActivity = &gClients[j].lastActivity;
                    break;
                }
            }

            int closeConn = 0;

            if (pfds[i].revents & (POLLHUP | POLLERR))
            {
                closeConn = 1;
            }
            else if (isAdminConn)
            {
                if (processAdminMessage(csock, cid) < 0)
                {
                    closeConn = 1;
                }
            }
            else
            {
                if (lastActivity == NULL ||
                    processClientMessage(csock, cid, lastActivity) < 0)
                {
                    closeConn = 1;
                }
            }

            if (closeConn)
            {
                (void)fprintf(stderr,
                              "[server] Client %u deconectat.\n", cid);
                if (isAdminConn)
                {
                    gAdminClientFd = -1;
                }
                removeClient(csock);
                pfds[i] = pfds[--nfds];
                i--;
            }
        }
    }
}

static void serverCleanup(int inetSock, int unixSock)
{
    for (int i = 0; i < gJobCount; i++)
    {
        if (gJobs[i].workerPid > 0)
        {
            (void)kill(gJobs[i].workerPid, SIGTERM);
        }
    }

    for (int i = 0; i < gClientCount; i++)
    {
        (void)close(gClients[i].sock);
    }

    if (inetSock >= 0)
    {
        (void)close(inetSock);
    }
    if (unixSock >= 0)
    {
        (void)close(unixSock);
        (void)unlink(gConfig.unixPath);
    }

    ocr_cleanup();

    if (gPdfCtx != NULL)
    {
        fz_drop_context(gPdfCtx);
        gPdfCtx = NULL;
    }

    (void)fprintf(stderr, "[server] Cleanup complet. La revedere.\n");
}

static int pdf_server_startup(int port)
{
    gConfig.port = port;

    if (loadConfig(NULL) < 0)
    {
        return -1;
    }

    ensureDir(gConfig.incomingDir);
    ensureDir(gConfig.outgoingDir);

    gPdfCtx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    if (gPdfCtx == NULL)
    {
        (void)fprintf(stderr, "[pdf] fz_new_context() esuat\n");
        return -1;
    }
    fz_try(gPdfCtx)
    {
        fz_register_document_handlers(gPdfCtx);
    }
    fz_catch(gPdfCtx)
    {
        (void)fprintf(stderr, "[pdf] fz_register_document_handlers() esuat\n");
        fz_drop_context(gPdfCtx);
        gPdfCtx = NULL;
        return -1;
    }

    if (ocr_init("ron+eng") < 0)
    {
        (void)fprintf(stderr, "[pdf] Avertisment: OCR dezactivat.\n");
    }
    if (gdpr_init() < 0)
    {
        (void)fprintf(stderr, "[pdf] Avertisment: GDPR redactor dezactivat.\n");
    }

    struct sigaction saChld;
    (void)memset(&saChld, 0, sizeof(saChld));
    saChld.sa_handler = handleSigchld;
    saChld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    (void)sigemptyset(&saChld.sa_mask);
    if (sigaction(SIGCHLD, &saChld, NULL) < 0)
    {
        perror("sigaction SIGCHLD");
        return -1;
    }

    struct sigaction saTerm;
    (void)memset(&saTerm, 0, sizeof(saTerm));
    saTerm.sa_handler = handleSigterm;
    (void)sigemptyset(&saTerm.sa_mask);
    if (sigaction(SIGTERM, &saTerm, NULL) < 0 ||
        sigaction(SIGINT, &saTerm, NULL) < 0)
    {
        perror("sigaction SIGTERM/SIGINT");
        return -1;
    }

    struct sigaction saPipe;
    (void)memset(&saPipe, 0, sizeof(saPipe));
    saPipe.sa_handler = SIG_IGN;
    (void)sigaction(SIGPIPE, &saPipe, NULL);

    if (startWorkerThread() < 0)
    {
        (void)fprintf(stderr, "[pdf] Avertisment: fir worker indisponibil.\n");
    }

    return 0;
}

void *pdf_main(void *args)
{
    int port = *(int *)args;
    int inetSock = -1;
    int unixSock = -1;

    if (pdf_server_startup(port) < 0)
    {
        pthread_exit(NULL);
    }

    inetSock = createInetSocket(gConfig.port);
    if (inetSock < 0)
    {
        serverCleanup(-1, -1);
        pthread_exit(NULL);
    }

    unixSock = createUnixSocket(gConfig.unixPath);
    if (unixSock < 0)
    {
        serverCleanup(inetSock, -1);
        pthread_exit(NULL);
    }

    gServerStartTime = time(NULL);
    (void)fprintf(stderr, "pdf_main: listening on port %d (unix: %s)\n",
                  gConfig.port, gConfig.unixPath);

    serverLoop(inetSock, unixSock);

    serverCleanup(inetSock, unixSock);
    pthread_exit(NULL);
}