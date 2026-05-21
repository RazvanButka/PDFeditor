/**
* Burbea Alexandru
* Header pentru protocolul PDF binar
* Defineste structurile, codurile de operatie si codurile de status pentru protocoul principal al serverului
*/
#ifndef PROTO_H
#define PROTO_H

#include <stdint.h> /* uint32_t, uint64_t */
#include <stddef.h> /* size_t */

//Extrage primii 32 biti dintr-un uint64_t
#define FSIZE_HIGH(sz) ((uint32_t)(((uint64_t)(sz)) >> 32))
//Extrage ultimii 32 biti dintr-un uint64_t
#define FSIZE_LOW(sz) ((uint32_t)(((uint64_t)(sz)) & 0xFFFFFFFFU))
//Combina doi uint32_t intr-un uint64_t
#define FSIZE_JOIN(h, l) ((uint64_t)(h) << 32 | (uint64_t)(l))

#define CHUNK_SIZE 4096 /**< Dimensiunea unui bloc de transfer (bytes). */


/**
* Coduri de operatio pentru toate cererile/raspunsurile.
* 0-9: Operatii de baza pe sesiune (BYE, OPEN, CLOSE)
* 10-15: Transfer fisiere
* 20-25: Procesare PDF
* 30-31: Status si informatii joburi
* 100-108: Operatii administrative (doar pentru client admin)
 */
typedef enum{
  OPR_BYE = 0,
  OPR_PDF_OPEN_DOC = 1,
  OPR_PDF_CLOSE = 2,
  OPR_UPLOAD_START = 10,
  OPR_UPLOAD_CHUNK = 11,
  OPR_UPLOAD_END = 12,
  OPR_DOWNLOAD_REQ = 13,
  OPR_DOWNLOAD_CHUNK = 14,
  OPR_DOWNLOAD_END = 15,
  OPR_PDF_OCR = 20,
  OPR_PDF_OCR_PAGE = 21,
  OPR_PDF_EXTRACT_TEXT = 22,
  OPR_PDF_CONVERT = 23,
  OPR_PDF_WATERMARK = 24,
  OPR_PDF_BLUR_GDPR = 25,
  OPR_STATUS_REQ = 30,
  OPR_JOB_LIST = 31,
  OPR_ADMIN_LIST_CLIENTS = 100,
  OPR_ADMIN_LIST_JOBS = 101,
  OPR_ADMIN_KICK_CLIENT = 102,
  OPR_ADMIN_KILL_JOB = 103,
  OPR_ADMIN_BLOCK_IP = 104,
  OPR_ADMIN_UNBLOCK_IP = 105,
  OPR_ADMIN_SYS_STATS = 106,
  OPR_ADMIN_JOB_HISTORY = 107,
  OPR_ADMIN_AVG_EXEC_TIME = 108,
} OpCode;

/**
* Coduri de status returnare de server 
* Permite clientului sa distinga rapid intre succes, esec sau stari
 */
typedef enum{
  STATUS_OK = 0,
  STATUS_ERROR = 1,
  STATUS_JOB_PENDING = 2,
  STATUS_JOB_PROCESSING = 3,
  STATUS_JOB_DONE = 4,
  STATUS_JOB_FAILED = 5,
  STATUS_JOB_KILLED = 6,
  STATUS_NOT_FOUND = 7,
  STATUS_UNAUTHORIZED = 8,
  STATUS_IP_BLOCKED = 9,
  STATUS_ALREADY_BLOCKED = 10,
  STATUS_UPLOAD_READY = 11,
  STATUS_UPLOAD_COMPLETE = 12,
  STATUS_DOWNLOAD_READY = 13,
  STATUS_ADMIN_ONLY = 14,
} StatusCode;

/**
* Formate de iesire pentru OPR_PDF_CNVERT si descarcare.
 */
typedef enum{
  FMT_KEEP = 0,
  FMT_TXT = 1,
  FMT_DOCX = 2,
  FMT_HTML = 3,
  FMT_MD = 4,
  FMT_RTF = 5,
}OutputFormat;

/**
* Header comun pentru toate mesajele protocolului PDF.
 */
typedef struct{
  uint32_t messageSize;
  uint32_t clientID;
  uint32_t opID;
  uint32_t statusCode;
} messageHeaderType;

//Mesaj simplu cu nume de fisier
typedef struct{
  messageHeaderType header;
  char fileName[256];
} pdfSimplemessageType;

//Mesaj de initiere upload
typedef struct{
  messageHeaderType header;
  char fileName[256]; //Numele original al fisierului
  uint32_t fileSizeHigh; // Dimensiunea fisierului - biti high
  uint32_t fileSizeLow; //Dimensiunea fisierului - biti low
  uint32_t opType; //Tipul operatiei 
  uint32_t outFmt; //Formatul de iesire dorit
  char opParam[256]; /**< watermark text, etc. */
} uploadStartmessageType;

//Mesaj de transfer bloc
typedef struct
{
  messageHeaderType header;
  uint32_t chunkLen; //Numarul de bytes valizi in data[]
  uint8_t data[CHUNK_SIZE]; //Bytes-ii fisierului
} uploadChunkmessageType;

//Confirmare upload de la server
typedef struct
{
  messageHeaderType header;
  uint32_t jobID;
} uploadAckmessageType;

//Cerere de status pentru un job
typedef struct
{
  messageHeaderType header;
  uint32_t jobID;
} jobStatusReqType;

//Raspuns la cererea de status - server informeaza clientul despre job
typedef struct
{
  messageHeaderType header;
  uint32_t jobID;
  uint32_t jobStatus;
  uint32_t progress;
} jobStatusRespType;

//Cerere de descarcare rezultat
typedef struct
{
  messageHeaderType header;
  uint32_t jobID;
  uint32_t outFmt;
} downloadReqType;

//Confirmare descarcare de la server - inceput de transfer
typedef struct
{
  messageHeaderType header;
  uint32_t fileSizeHigh;
  uint32_t fileSizeLow;
  char fileName[256];
} downloadAckType;

//Cerere admin: deconectare fortata client
typedef struct
{
  messageHeaderType header;
  uint32_t targetClientID;
} adminKickmessageType;

//Cerere admin: terminare fortata job
typedef struct
{
  messageHeaderType header;
  uint32_t targetJobID;
} adminKillJobmessageType;

//Cerere admin: gestionare blacklist IP
typedef struct
{
  messageHeaderType header;
  char ipAddr[46];
} adminIPmessageType;

messageHeaderType peekmessageHeader(int sock);
int send_all(int sock, const void *buf, size_t len);
int recv_all(int sock, void *buf, size_t len);

#endif /* PROTO_H */