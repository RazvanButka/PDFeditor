#ifndef PROTO_INET_H
#define PROTO_INET_H

/* Protocol INET simplu (port 18081) — separat de protocolul PDF binar. */

#define OPR_CONNECT 0
#define OPR_ECHO 1
#define OPR_CONC 2
#define OPR_NEG 3
#define OPR_ADD 4
#define OPR_BYE 5

typedef struct msgHeader
{
    int msgSize;
    int clientID;
    int opID;
} msgHeaderType;

typedef struct int2Msg
{
    int msg1, msg2;
} msg2IntType;

typedef struct intMsg
{
    int msg;
} msgIntType;

typedef struct singleIntMsg
{
    msgHeaderType header;
    msgIntType i;
} singleIntMsgType;

typedef struct multiIntMsg
{
    msgHeaderType header;
    msg2IntType i;
} multiIntMsgType;

typedef struct stringMsg
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
