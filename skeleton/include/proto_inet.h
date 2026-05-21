/**
 * Header pentru protocolul INET simplut 
 * Acest protocol e separat de cel folosit de serverul PDF
 * Foloseste mesaje binare cu header fix
 */
#ifndef PROTO_INET_H
#define PROTO_INET_H

/* Protocol INET simplu (port 18081) — separat de protocolul PDF binar. */

#define OPR_CONNECT 0 // Cerere de conectare
#define OPR_ECHO 1 // Serverul returneaza string-ul primit
#define OPR_CONC 2 // Concatenare
#define OPR_NEG 3 // Serverul returneaza negativul intregului primit
#define OPR_ADD 4 // Serverul returneaza suma a doi intregi
#define OPR_BYE 5 //Clientul inchide conexiunea


typedef struct msgHeader //Header comun pentru toate mesajele INET
{
    int msgSize; 
    int clientID;
    int opID;
} msgHeaderType;

typedef struct int2Msg //Payload cu 2 intregi folosit pentru operatii cu 2 operanzi
{
    int msg1, msg2;
} msg2IntType;

typedef struct intMsg //Payload cu un singur intreg
{
    int msg;
} msgIntType;

typedef struct singleIntMsg //Mesaj complet cu un singur intreg header+payload
{
    msgHeaderType header;
    msgIntType i;
} singleIntMsgType;

typedef struct multiIntMsg //Mesaj complet cu 2 intregi
{
    msgHeaderType header;
    msg2IntType i;
} multiIntMsgType;

typedef struct stringMsg //Payload cu string de lungime variabila
{
    char *msg;
} msgStringType;

msgHeaderType peekMsgHeader(int sock);
int readSingleInt(int sock, msgIntType *m);
int readMultiInt(int sock, msgIntType *m1, msgIntType *m2);
int readSingleString(int sock, msgStringType *m);
int writeSingleInt(int sock, msgHeaderType h, int i);
int writeMultiInt(int sock, msgHeaderType h, int i1, int i2);
int writeSingleString(int sock, msgHeaderType h, char *s);

#endif /* PROTO_INET_H */
