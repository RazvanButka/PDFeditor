#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "proto.h"

#define PDF_SERVER "127.0.0.1"
#define PDF_PORT 18083

int connect_to_pdf_server()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serverAddr;
    if (sock < 0)
    {
        perror("Socket error");
        exit(1);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PDF_PORT);

    inet_pton(AF_INET, PDF_SERVER, &serverAddr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("Connection error");
        exit(1);
    }
    printf("Connected to pdf server at %s:%d", PDF_SERVER, PDF_PORT);

    return sock;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s pdf file", argv[0]);
        return 1;
    }

    int sock = connect_to_pdf_server();
    pdfSimpleMsgType openMsg;
    openMsg.header.msgSize = htonl(sizeof(openMsg));
    openMsg.header.clientID = htonl(0);
    openMsg.header.opID = htonl(OPR_PDF_OPEN_DOC);

    strncpy(openMsg.fileName, argv[1], 255);
    openMsg.fileName[255] = '\0';

    if (send(sock, &openMsg, sizeof(openMsg), 0) < 0)
    {
        perror("Close");
        exit(1);
    }

    msgHeaderType response;
    int pageCount;

    if (recv(sock, &response, sizeof(response), MSG_WAITALL) < 0 || recv(sock, &pageCount, sizeof(int), MSG_WAITALL) < 0)
    {
        perror("Close socket");
        return 1;
    }

    pageCount = ntohl(pageCount);
    if (pageCount >= 0)
    {
        printf("SUCCES with %d", pageCount);
    }
    else
    {
        printf("Fail");
    }

    pdfSimpleMsgType ocrMsg;  //Tesseract 
    ocrMsg.header.msgSize = htonl(sizeof(ocrMsg));
    ocrMsg.header.clientID = htonl(0);
    ocrMsg.header.opID = htonl(OPR_PDF_OCR);
    strncpy(ocrMsg.fileName, argv[1], 255);
    ocrMsg.fileName[255] = '\0';

    if (send(sock, &ocrMsg, sizeof(ocrMsg), 0) < 0) {
        perror("OCR send failed");
        exit(1);
    }

    msgHeaderType ocrResponse;
    int textLen;
    if (recv(sock, &ocrResponse, sizeof(ocrResponse), MSG_WAITALL) < 0) {
        perror("OCR response header failed");
        return 1;
    }
    if (recv(sock, &textLen, sizeof(int), MSG_WAITALL) < 0) {
        perror("OCR text length failed");
        return 1;
    }
    textLen = ntohl(textLen);
    
    if (textLen > 0) {
        char *text = malloc(textLen + 1);
        if (!text) {
            perror("malloc OCR buffer");
            return 1;
        }
        if (recv(sock, text, textLen, MSG_WAITALL) < 0) {
            perror("OCR text recv failed");
            free(text);
            return 1;
        }
        text[textLen] = '\0';
        printf("\n=== OCR result ===\n%s\n=== End OCR ===\n", text);
        free(text);
    } else {
        printf("\nOCR returned no text\n");
    } //Tesseract 

    pdfSimpleMsgType closeMsg;
    closeMsg.header.msgSize = htonl(sizeof(closeMsg));
    closeMsg.header.clientID = htonl(0);
    closeMsg.header.opID = htonl(OPR_PDF_CLOSE);

    strncpy(closeMsg.fileName, argv[1], 255);
    closeMsg.fileName[255] = '\0';

    if (send(sock, &closeMsg, sizeof(closeMsg), 0) < 0)
    {
        perror("Fail");
        exit(1);
    }

    if (recv(sock, &response, sizeof(response), MSG_WAITALL) < 0)
    {
        perror("Failed to receive response");
        return 1;
    }

    printf("Success PDF Closed");
    return 0;
}