/*
* Programul de mai jos implementeaza un client TCP care se conecteaza la un server PDF
* si trimite cereri pentru deschiderea si inchiderea unui fisier PDF
* Clientul trimite numele fisierului catre server, primeste numarul de pagini,
* si apoi solicita inchiderea documentului
* Printre situatiile limita tratate se numara:
numarul insuficient de argumente, trimiterea/primirea incompleta a datelor
*/

#include <stdio.h> //input output
#include <stdlib.h> //exit
#include <string.h> //manipulare string
#include <unistd.h> //Apeluri POSIX (close)
#include <sys/types.h> //tiprui de socketuri
#include <sys/socket.h> //pt implementarea functiilor socket, connect,etc
#include <netinet/in.h> //structuri pentru adrese IP
#include <arpa/inet.h> //functiile inet_pton, htons
#include "proto.h" //defintii pentru protocolul de comunicare

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