#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "proto.h"

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 18083
#define STATUS_POLL_INTERVAL_MS 2000
#define POLL_TIMEOUT_MS 30000
#define MAX_POLL_RETRIES 60

typedef struct
{
    char host[64];
    uint16_t port;
    char file_path[512];
    uint32_t operation;
    uint32_t out_fmt;
    uint32_t download_job_id;
    char watermark_text[256];
    int verbose;
} ClientConfig;

static void print_env_info(void)
{
    const char *vars[] = {"HOME", "USER", "PDF_SERVER", "PDF_PORT", NULL};
    (void)printf("\n--- Variabile de mediu relevante ---\n");
    for (int i = 0; vars[i] != NULL; i++)
    {
        const char *val = getenv(vars[i]);
        (void)printf("  %s = %s\n", vars[i], val ? val : "(nedefinit)");
    }
    (void)printf("------------------------------------\n\n");
}

static void usage(const char *prog)
{
    (void)fprintf(stderr,
                  "Utilizare: %s -h <host> -p <port> -f <fisier.pdf> -o <operatie>\n"
                  "           [-F <format>] [-w <watermark>] [-d <job_id>] [-v]\n\n"
                  "  -h  Adresa serverului  (implicit: " DEFAULT_HOST ")\n"
                  "  -p  Port server        (implicit: %d)\n"
                  "  -f  Fisier PDF de uplodat\n"
                  "  -o  Operatie: ocr | text | watermark | blur | convert\n"
                  "  -F  Format iesire: keep | txt | docx | html | md | rtf\n"
                  "  -w  Text watermark (folosit cu -o watermark)\n"
                  "  -d  Job ID pentru download direct (fara upload)\n"
                  "  -v  Verbose\n\n"
                  "Exemple:\n"
                  "  %s -h 127.0.0.1 -p 18083 -f doc.pdf -o ocr -F docx\n"
                  "  %s -h 127.0.0.1 -p 18083 -d 42 -F txt\n",
                  prog, DEFAULT_PORT, prog, prog);
}

static int parse_args(int argc, char *argv[], ClientConfig *cfg)
{
    (void)strncpy(cfg->host, DEFAULT_HOST, sizeof(cfg->host) - 1);
    cfg->port = DEFAULT_PORT;
    cfg->file_path[0] = '\0';
    cfg->operation = OPR_PDF_OCR;
    cfg->out_fmt = FMT_KEEP;
    cfg->download_job_id = 0;
    cfg->watermark_text[0] = '\0';
    cfg->verbose = 0;

    int opt;
    while ((opt = getopt(argc, argv, "h:p:f:o:F:w:d:v")) != -1)
    {
        switch (opt)
        {
        case 'h':
            (void)strncpy(cfg->host, optarg, sizeof(cfg->host) - 1);
            break;
        case 'p':
        {
            long p = strtol(optarg, NULL, 10);
            if (p <= 0 || p > 65535)
            {
                (void)fprintf(stderr, "Port invalid: %s\n", optarg);
                return -1;
            }
            cfg->port = (uint16_t)p;
            break;
        }
        case 'f':
            (void)strncpy(cfg->file_path, optarg,
                          sizeof(cfg->file_path) - 1);
            break;
        case 'o':
            if (strcmp(optarg, "ocr") == 0)
                cfg->operation = OPR_PDF_OCR;
            else if (strcmp(optarg, "text") == 0)
                cfg->operation = OPR_PDF_EXTRACT_TEXT;
            else if (strcmp(optarg, "watermark") == 0)
                cfg->operation = OPR_PDF_WATERMARK;
            else if (strcmp(optarg, "blur") == 0)
                cfg->operation = OPR_PDF_BLUR_GDPR;
            else if (strcmp(optarg, "convert") == 0)
                cfg->operation = OPR_PDF_CONVERT;
            else
            {
                (void)fprintf(stderr, "Operatie necunoscuta: %s\n", optarg);
                return -1;
            }
            break;
        case 'F':
            if (strcmp(optarg, "keep") == 0)
                cfg->out_fmt = FMT_KEEP;
            else if (strcmp(optarg, "txt") == 0)
                cfg->out_fmt = FMT_TXT;
            else if (strcmp(optarg, "docx") == 0)
                cfg->out_fmt = FMT_DOCX;
            else if (strcmp(optarg, "html") == 0)
                cfg->out_fmt = FMT_HTML;
            else if (strcmp(optarg, "md") == 0)
                cfg->out_fmt = FMT_MD;
            else if (strcmp(optarg, "rtf") == 0)
                cfg->out_fmt = FMT_RTF;
            else
            {
                (void)fprintf(stderr, "Format necunoscut: %s\n", optarg);
                return -1;
            }
            break;
        case 'w':
            (void)strncpy(cfg->watermark_text, optarg,
                          sizeof(cfg->watermark_text) - 1);
            break;
        case 'd':
        {
            long jid = strtol(optarg, NULL, 10);
            if (jid <= 0)
            {
                (void)fprintf(stderr, "Job ID invalid: %s\n", optarg);
                return -1;
            }
            cfg->download_job_id = (uint32_t)jid;
            break;
        }
        case 'v':
            cfg->verbose = 1;
            break;
        default:
            usage(argv[0]);
            return -1;
        }
    }

    if (cfg->file_path[0] == '\0' && cfg->download_job_id == 0)
    {
        (void)fprintf(stderr,
                      "Eroare: specificati -f <fisier> sau -d <job_id>.\n");
        usage(argv[0]);
        return -1;
    }

    return 0;
}

static int connect_to_server(const char *host, uint16_t port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket()");
        return -1;
    }

    struct sockaddr_in addr;
    (void)memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1)
    {
        (void)fprintf(stderr, "Adresa IP invalida: %s\n", host);
        (void)close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect()");
        (void)close(sock);
        return -1;
    }

    (void)printf("[client] Conectat la %s:%" PRIu16 "\n", host, port);
    return sock;
}

static uint32_t do_upload(int sock, const ClientConfig *cfg,
                          uint32_t client_id)
{
    struct stat st;
    if (stat(cfg->file_path, &st) < 0)
    {
        (void)fprintf(stderr, "stat(%s): %s\n",
                      cfg->file_path, strerror(errno));
        return 0;
    }
    uint64_t file_size = (uint64_t)st.st_size;

    uploadStartmessageType start_msg;
    (void)memset(&start_msg, 0, sizeof(start_msg));
    start_msg.header.messageSize = htonl((uint32_t)sizeof(start_msg));
    start_msg.header.clientID = htonl(client_id);
    start_msg.header.opID = htonl(OPR_UPLOAD_START);
    start_msg.header.statusCode = htonl(STATUS_OK);

    const char *basename = strrchr(cfg->file_path, '/');
    basename = (basename != NULL) ? basename + 1 : cfg->file_path;
    (void)strncpy(start_msg.fileName, basename,
                  sizeof(start_msg.fileName) - 1);

    start_msg.fileSizeHigh = htonl(FSIZE_HIGH(file_size));
    start_msg.fileSizeLow = htonl(FSIZE_LOW(file_size));
    start_msg.opType = htonl(cfg->operation);
    start_msg.outFmt = htonl(cfg->out_fmt);
    (void)memset(start_msg.opParam, 0, sizeof(start_msg.opParam));
    if (cfg->operation == OPR_PDF_WATERMARK && cfg->watermark_text[0] != '\0')
    {
        (void)strncpy(start_msg.opParam, cfg->watermark_text,
                      sizeof(start_msg.opParam) - 1);
    }

    if (send_all(sock, &start_msg, sizeof(start_msg)) < 0)
    {
        (void)fprintf(stderr, "do_upload: trimitere UPLOAD_START esuata\n");
        return 0;
    }
    (void)printf("[upload] Initiat: %s (%" PRIu64 " bytes)\n",
                 basename, file_size);

    uploadAckmessageType ack;
    if (recv_all(sock, &ack, sizeof(ack)) < 0)
    {
        (void)fprintf(stderr, "do_upload: recv ACK UPLOAD_START esuat\n");
        return 0;
    }
    uint32_t status = ntohl(ack.header.statusCode);
    if (status != STATUS_UPLOAD_READY)
    {
        (void)fprintf(stderr,
                      "do_upload: server refuzat upload (status=%u)\n",
                      status);
        return 0;
    }
    uint32_t job_id = ntohl(ack.jobID);
    (void)printf("[upload] Server gata. Job ID preallocat: %u\n", job_id);

    int fd = open(cfg->file_path, O_RDONLY);
    if (fd < 0)
    {
        (void)fprintf(stderr, "open(%s): %s\n",
                      cfg->file_path, strerror(errno));
        return 0;
    }

    uploadChunkmessageType chunk_msg;
    uint64_t sent_bytes = 0;
    ssize_t bytes_read;

    while ((bytes_read = read(fd, chunk_msg.data, CHUNK_SIZE)) > 0)
    {
        (void)memset(&chunk_msg.header, 0, sizeof(chunk_msg.header));
        chunk_msg.header.messageSize =
            htonl((uint32_t)(sizeof(messageHeaderType) +
                             sizeof(uint32_t) + (uint32_t)bytes_read));
        chunk_msg.header.clientID = htonl(client_id);
        chunk_msg.header.opID = htonl(OPR_UPLOAD_CHUNK);
        chunk_msg.header.statusCode = htonl(STATUS_OK);
        chunk_msg.chunkLen = htonl((uint32_t)bytes_read);

        size_t total_chunk = sizeof(messageHeaderType) + sizeof(uint32_t) +
                             (size_t)bytes_read;
        if (send_all(sock, &chunk_msg, total_chunk) < 0)
        {
            (void)fprintf(stderr, "do_upload: send chunk esuat\n");
            (void)close(fd);
            return 0;
        }
        sent_bytes += (uint64_t)bytes_read;

        if (cfg->verbose)
        {
            (void)printf("\r[upload] %" PRIu64 " / %" PRIu64 " bytes (%.1f%%)",
                         sent_bytes, file_size,
                         file_size > 0
                             ? (double)sent_bytes * 100.0 / (double)file_size
                             : 100.0);
            (void)fflush(stdout);
        }
    }
    if (bytes_read < 0)
    {
        perror("read fisier");
        (void)close(fd);
        return 0;
    }
    (void)close(fd);
    (void)printf("\n[upload] %" PRIu64 " bytes transmisi.\n", sent_bytes);

    messageHeaderType end_hdr;
    (void)memset(&end_hdr, 0, sizeof(end_hdr));
    end_hdr.messageSize = htonl((uint32_t)sizeof(end_hdr));
    end_hdr.clientID = htonl(client_id);
    end_hdr.opID = htonl(OPR_UPLOAD_END);
    end_hdr.statusCode = htonl(STATUS_OK);

    if (send_all(sock, &end_hdr, sizeof(end_hdr)) < 0)
    {
        (void)fprintf(stderr, "do_upload: send UPLOAD_END esuat\n");
        return 0;
    }

    if (recv_all(sock, &ack, sizeof(ack)) < 0)
    {
        (void)fprintf(stderr, "do_upload: recv ACK UPLOAD_END esuat\n");
        return 0;
    }
    status = ntohl(ack.header.statusCode);
    if (status != STATUS_UPLOAD_COMPLETE)
    {
        (void)fprintf(stderr,
                      "do_upload: upload incomplet (status=%u)\n", status);
        return 0;
    }

    job_id = ntohl(ack.jobID);
    (void)printf("[upload] Finalizat. Job ID confirmat: %u\n", job_id);
    return job_id;
}

static int wait_for_job_done(int sock, uint32_t job_id, uint32_t client_id)
{
    struct pollfd pfd;
    pfd.fd = sock;
    pfd.events = POLLIN | POLLHUP | POLLERR;

    for (int attempt = 0; attempt < MAX_POLL_RETRIES; attempt++)
    {
        jobStatusReqType req;
        (void)memset(&req, 0, sizeof(req));
        req.header.messageSize = htonl((uint32_t)sizeof(req));
        req.header.clientID = htonl(client_id);
        req.header.opID = htonl(OPR_STATUS_REQ);
        req.header.statusCode = htonl(STATUS_OK);
        req.jobID = htonl(job_id);

        if (send_all(sock, &req, sizeof(req)) < 0)
        {
            (void)fprintf(stderr,
                          "[status] Eroare trimitere cerere status.\n");
            return 0;
        }

        int ret = poll(&pfd, 1, STATUS_POLL_INTERVAL_MS);
        if (ret < 0)
        {
            perror("poll()");
            return 0;
        }
        if (ret == 0)
        {
            (void)printf("[status] Job %u: asteptam... (%d/%d)\n",
                         job_id, attempt + 1, MAX_POLL_RETRIES);
            continue;
        }

        if (pfd.revents & (POLLHUP | POLLERR))
        {
            (void)fprintf(stderr, "[status] Serverul s-a deconectat.\n");
            return 0;
        }

        jobStatusRespType resp;
        if (recv_all(sock, &resp, sizeof(resp)) < 0)
        {
            (void)fprintf(stderr, "[status] Eroare receptie raspuns.\n");
            return 0;
        }

        uint32_t js = ntohl(resp.jobStatus);
        uint32_t pr = ntohl(resp.progress);

        (void)printf("[status] Job %u: %s (%u%%)\n",
                     job_id,
                     js == STATUS_JOB_DONE ? "DONE" : js == STATUS_JOB_PROCESSING ? "PROCESSING"
                                                  : js == STATUS_JOB_PENDING      ? "PENDING"
                                                  : js == STATUS_JOB_FAILED       ? "FAILED"
                                                  : js == STATUS_JOB_KILLED       ? "KILLED"
                                                                                  : "UNKNOWN",
                     pr);

        if (js == STATUS_JOB_DONE)
        {
            return 1;
        }
        if (js == STATUS_JOB_FAILED || js == STATUS_JOB_KILLED)
        {
            (void)fprintf(stderr, "[status] Job %u terminat cu eroare.\n",
                          job_id);
            return 0;
        }

        struct timespec ts = {STATUS_POLL_INTERVAL_MS / 1000,
                              (STATUS_POLL_INTERVAL_MS % 1000) * 1000000L};
        (void)nanosleep(&ts, NULL);
    }

    (void)fprintf(stderr, "[status] Timeout asteptare job %u.\n", job_id);
    return 0;
}

static int do_download(int sock, uint32_t job_id, uint32_t out_fmt,
                       uint32_t client_id)
{
    downloadReqType req;
    (void)memset(&req, 0, sizeof(req));
    req.header.messageSize = htonl((uint32_t)sizeof(req));
    req.header.clientID = htonl(client_id);
    req.header.opID = htonl(OPR_DOWNLOAD_REQ);
    req.header.statusCode = htonl(STATUS_OK);
    req.jobID = htonl(job_id);
    req.outFmt = htonl(out_fmt);

    if (send_all(sock, &req, sizeof(req)) < 0)
    {
        (void)fprintf(stderr, "[download] Eroare trimitere cerere.\n");
        return -1;
    }

    downloadAckType ack;
    if (recv_all(sock, &ack, sizeof(ack)) < 0)
    {
        (void)fprintf(stderr, "[download] Eroare receptie ACK.\n");
        return -1;
    }

    uint32_t status = ntohl(ack.header.statusCode);
    if (status == STATUS_JOB_PROCESSING || status == STATUS_JOB_PENDING)
    {
        (void)fprintf(stderr,
                      "[download] Job %u nu este finalizat inca.\n", job_id);
        return -1;
    }
    if (status != STATUS_DOWNLOAD_READY)
    {
        (void)fprintf(stderr,
                      "[download] Server a refuzat (status=%u).\n", status);
        return -1;
    }

    uint64_t total_size = FSIZE_JOIN(ntohl(ack.fileSizeHigh),
                                     ntohl(ack.fileSizeLow));
    ack.fileName[sizeof(ack.fileName) - 1] = '\0';
    (void)printf("[download] Fisier: %s (%" PRIu64 " bytes)\n",
                 ack.fileName, total_size);

    int out_fd = open(ack.fileName,
                      O_WRONLY | O_CREAT | O_TRUNC,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (out_fd < 0)
    {
        (void)fprintf(stderr, "open(%s) local: %s\n",
                      ack.fileName, strerror(errno));
        return -1;
    }

    uint64_t received = 0;
    uploadChunkmessageType chunk;

    while (received < total_size)
    {
        if (recv_all(sock, &chunk.header, sizeof(chunk.header)) < 0)
        {
            (void)fprintf(stderr, "[download] Eroare receptie antet bloc.\n");
            (void)close(out_fd);
            return -1;
        }

        uint32_t op = ntohl(chunk.header.opID);

        if (op == OPR_DOWNLOAD_END)
        {
            break;
        }
        if (op != OPR_DOWNLOAD_CHUNK)
        {
            (void)fprintf(stderr,
                          "[download] OpCode neasteptat: %u\n", op);
            (void)close(out_fd);
            return -1;
        }

        if (recv_all(sock, &chunk.chunkLen, sizeof(chunk.chunkLen)) < 0)
        {
            (void)fprintf(stderr, "[download] Eroare receptie chunkLen.\n");
            (void)close(out_fd);
            return -1;
        }
        uint32_t chunk_len = ntohl(chunk.chunkLen);
        if (chunk_len == 0 || chunk_len > CHUNK_SIZE)
        {
            (void)fprintf(stderr,
                          "[download] Dimensiune bloc invalida: %u\n",
                          chunk_len);
            (void)close(out_fd);
            return -1;
        }

        if (recv_all(sock, chunk.data, chunk_len) < 0)
        {
            (void)fprintf(stderr, "[download] Eroare receptie date bloc.\n");
            (void)close(out_fd);
            return -1;
        }

        ssize_t wr = write(out_fd, chunk.data, chunk_len);
        if (wr < 0 || (size_t)wr != chunk_len)
        {
            perror("write fisier local");
            (void)close(out_fd);
            return -1;
        }
        received += chunk_len;
        (void)printf("\r[download] %" PRIu64 " / %" PRIu64 " bytes (%.1f%%)",
                     received, total_size,
                     total_size > 0
                         ? (double)received * 100.0 / (double)total_size
                         : 100.0);
        (void)fflush(stdout);
    }

    (void)close(out_fd);
    (void)printf("\n[download] Complet → %s\n", ack.fileName);
    return 0;
}

int main(int argc, char *argv[])
{
    ClientConfig cfg;
    if (parse_args(argc, argv, &cfg) < 0)
    {
        return EXIT_FAILURE;
    }

    if (cfg.verbose)
    {
        print_env_info();
    }

    int sock = connect_to_server(cfg.host, cfg.port);
    if (sock < 0)
    {
        return EXIT_FAILURE;
    }

    messageHeaderType hello;
    uint32_t client_id = 0;
    if (recv_all(sock, &hello, sizeof(hello)) == 0)
    {
        client_id = ntohl(hello.clientID);
        (void)printf("[client] ClientID alocat de server: %u\n", client_id);
    }
    else
    {
        (void)fprintf(stderr, "[client] Nu am primit clientID de la server.\n");
    }

    uint32_t job_id = cfg.download_job_id;
    int rc = 0;

    if (cfg.file_path[0] != '\0')
    {
        job_id = do_upload(sock, &cfg, client_id);
        if (job_id == 0)
        {
            (void)fprintf(stderr, "[client] Upload esuat.\n");
            (void)close(sock);
            return EXIT_FAILURE;
        }

        if (!wait_for_job_done(sock, job_id, client_id))
        {
            (void)fprintf(stderr, "[client] Procesare esuata/timeout.\n");
            (void)close(sock);
            return EXIT_FAILURE;
        }
    }

    if (job_id > 0)
    {
        rc = do_download(sock, job_id, cfg.out_fmt, client_id);
        if (rc < 0)
        {
            (void)fprintf(stderr, "[client] Download esuat.\n");
        }
    }

    messageHeaderType bye;
    (void)memset(&bye, 0, sizeof(bye));
    bye.messageSize = htonl((uint32_t)sizeof(bye));
    bye.clientID = htonl(client_id);
    bye.opID = htonl(OPR_BYE);
    (void)send_all(sock, &bye, sizeof(bye));

    (void)close(sock);
    (void)printf("[client] Deconectat.\n");
    return (rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}