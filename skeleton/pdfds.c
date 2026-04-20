#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mupdf/fitz.h>
#include <libconfig.h>
#include "proto.h"
#include "pdf_ocr.h" //Pt Tesseract 

#define PDF_PORT 18083

static fz_context *pdf_ctx = NULL;
static fz_document *currentDoc = NULL;

void pdf_init()
{
    pdf_ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    if (!pdf_ctx)
    {
        fprintf(stderr, "Fail to connect context");
        exit(1);
    }

    fz_try(pdf_ctx)
    {
        fz_register_document_handlers(pdf_ctx);
    }
    fz_catch(pdf_ctx)
    {
        fprintf(stderr, "Fail to register handler");
        exit(1);
    }
    if (ocr_init("ron+eng") < 0) { // Tesseract
        fprintf(stderr, "Warning: OCR disabled\n");
    }
}

int pdf_open(char *fileName)
{
    if (!pdf_ctx || !fileName)
    {
        return -1;
    }

    int pageCount = -1;
    fz_try(pdf_ctx)
    {
        currentDoc = fz_open_document(pdf_ctx, fileName);
        pageCount = fz_count_pages(pdf_ctx, currentDoc);
        printf("PDF open: %s, %d pages", fileName, pageCount);
    }
    fz_catch(pdf_ctx)
    {
        fprintf(stderr, "Fail to open %s", fileName);
    }

    return pageCount;
}

void pdf_close()
{
    if (!pdf_ctx || !currentDoc)
    {
        return;
    }

    fz_try(pdf_ctx)
    {
        fz_drop_document(pdf_ctx, currentDoc);
        currentDoc = NULL;
        printf("Document Closed");
    }
    fz_catch(pdf_ctx)
    {
        fprintf(stderr, "Error closing current document");
    }
}

void pdfHandleClient(int sock)
{
    msgHeaderType header;
    pdfSimpleMsgType msg;
    int result = 0;
    printf("pdfHandleClient:newConnection");

    while (1)
    {
        header = peekMsgHeader(sock);
        if (header.clientID == -1 || header.opID == OPR_BYE)
        {
            break;
        }
        switch (header.opID)
        {
        case OPR_PDF_OPEN_DOC:
        {
            recv(sock, &msg, sizeof(msg), MSG_WAITALL);
            result = pdf_open(msg.fileName);
            msgHeaderType resp;
            resp.msgSize = htonl(sizeof(msgHeaderType) + sizeof(int));
            resp.clientID = msg.header.clientID;
            resp.opID = htonl(OPR_PDF_OPEN_DOC);
            send(sock, &resp, sizeof(msgHeaderType), 0);
            int x = htonl(result);
            send(sock, &x, sizeof(int), 0);
            printf("Send page count: %d", result);
            break;
        }
        case OPR_PDF_CLOSE:
        {
            recv(sock, &msg, sizeof(msg), MSG_WAITALL);
            pdf_close();
            msgHeaderType response;
            response.msgSize = htonl(sizeof(msgHeaderType));
            response.clientID = msg.header.clientID;
            response.opID = htonl(OPR_PDF_CLOSE);
            send(sock, &response, sizeof(msgHeaderType), 0);
            printf("Document closed.");
            goto cleanup;
            break;
        } 
        case OPR_PDF_OCR: //Tesseract 
        {
            recv(sock, &msg, sizeof(msg), MSG_WAITALL);
            char *text = ocr_extract_all_text(pdf_ctx, currentDoc, 300);
    
            msgHeaderType resp;
            int len = text ? strlen(text) : 0;
            resp.msgSize = htonl(sizeof(msgHeaderType) + sizeof(int) + len);
            resp.clientID = msg.header.clientID;
            resp.opID = htonl(OPR_PDF_OCR);
            send(sock, &resp, sizeof(msgHeaderType), 0);
    
            int nlen = htonl(len);
            send(sock, &nlen, sizeof(int), 0);
            if (text) {
                send(sock, text, len, 0);
                free(text);
            }
            break;
        }
        default:
            break;
        }
    }
cleanup:
    close(sock);
    printf("Connection closed");
}

void pdf_main()
{
    int port = PDF_PORT;
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddrLen;
    int useAddrOn = 1;
    // pthread_t threadID;

    pdf_init();
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket < 0)
    {
        perror("Server Socket Failed");
        goto cleanup;
    }

    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &useAddrOn, sizeof(useAddrOn)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PDF_PORT);

    if (bind(serverSocket, (struct sockaddr *)&serverAddr,sizeof(serverAddr)) < 0)
    {
        perror("Bind");
        close(serverSocket);
        goto cleanup;
    }

    if (listen(serverSocket, 5) < 0)
    {
        perror("Listen");
        close(serverSocket);
        goto cleanup;
    }

    fprintf(stderr, "pdf_main: Listening on port %d\n", port);

    for (;;)
    {
        clientAddrLen = sizeof(clientAddr);
        clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);

        if (clientSocket < 0)
        {
            perror("Accept");
            continue;
        }
        fprintf(stderr, "pdf_main: Connection from: %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

        pdfHandleClient(clientSocket);

        // *pclient = clientSocket;

        // if(pthread_create(&threadID, NULL, pdfHandleClient, pclient) != 0){
        //     perror("Thread creation has failed");
        //     close(clientSocket);
        //     free(pclient);
        // }
        // pthread_detach(threadID);
    }

cleanup:
    if (pdf_ctx)
        fz_drop_context(pdf_ctx);
    pthread_exit(NULL);
}
