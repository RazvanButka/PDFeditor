#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "proto_inet.h"

msgHeaderType peekMsgHeader(int sock)
{
    size_t nb;
    msgHeaderType h;
    h.msgSize = (int)htonl((uint32_t)sizeof(h));
    nb = recv(sock, &h, sizeof(h), MSG_PEEK | MSG_WAITALL);
    h.msgSize = (int)ntohl((uint32_t)h.msgSize);
    h.clientID = (int)ntohl((uint32_t)h.clientID);
    h.opID = (int)ntohl((uint32_t)h.opID);
    if (nb == (size_t)-1)
    {
        h.opID = h.clientID = -1;
    }
    if (nb == 0)
    {
        h.opID = h.clientID = OPR_BYE;
    }
    return h;
}

int readSingleInt(int sock, msgIntType *m)
{
    size_t nb;
    singleIntMsgType s;
    nb = recv(sock, &s, sizeof(s), MSG_WAITALL);
    if (nb <= 0)
    {
        m->msg = -1;
        return -1;
    }
    m->msg = (int)ntohl((uint32_t)s.i.msg);
    return (int)nb;
}

int writeSingleInt(int sock, msgHeaderType h, int i)
{
    singleIntMsgType s;
    s.header.clientID = (int)htonl((uint32_t)h.clientID);
    s.header.opID = (int)htonl((uint32_t)h.opID);
    s.i.msg = (int)htonl((uint32_t)i);
    s.header.msgSize = (int)htonl((uint32_t)sizeof(s));
    size_t nb = send(sock, &s, sizeof(s), 0);
    if (nb <= 0)
    {
        return -1;
    }
    return (int)nb;
}

int readMultiInt(int sock, msgIntType *m1, msgIntType *m2)
{
    size_t nb;
    multiIntMsgType s;
    nb = recv(sock, &s, sizeof(s), MSG_WAITALL);
    if (nb <= 0)
    {
        m1->msg = m2->msg = -1;
        return -1;
    }
    m1->msg = (int)ntohl((uint32_t)s.i.msg1);
    m2->msg = (int)ntohl((uint32_t)s.i.msg2);
    return (int)nb;
}

int writeMultiInt(int sock, msgHeaderType h, int i1, int i2)
{
    multiIntMsgType s;
    s.header.clientID = (int)htonl((uint32_t)h.clientID);
    s.header.opID = (int)htonl((uint32_t)h.opID);
    s.i.msg1 = (int)htonl((uint32_t)i1);
    s.i.msg2 = (int)htonl((uint32_t)i2);
    s.header.msgSize = (int)htonl((uint32_t)sizeof(s));
    size_t nb = send(sock, &s, sizeof(s), 0);
    if (nb <= 0)
    {
        return -1;
    }
    return (int)nb;
}

int readSingleString(int sock, msgStringType *str)
{
    size_t nb;
    msgIntType m;
    nb = (size_t)readSingleInt(sock, &m);
    if (nb == (size_t)-1 || m.msg < 0)
    {
        return -1;
    }
    str->msg = (char *)malloc((size_t)m.msg + 1);
    if (str->msg == NULL)
    {
        return -1;
    }
    nb = recv(sock, str->msg, (size_t)m.msg, MSG_WAITALL);
    str->msg[m.msg] = '\0';
    return (int)nb;
}

int writeSingleString(int sock, msgHeaderType h, char *str)
{
    size_t nb;
    int strSize = (int)strlen(str);
    nb = (size_t)writeSingleInt(sock, h, strSize);
    if (nb == (size_t)-1 || nb == 0)
    {
        return -1;
    }
    nb = send(sock, str, (size_t)strSize, 0);
    return (nb <= 0) ? -1 : (int)nb;
}
