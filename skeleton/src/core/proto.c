#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "proto.h"

#define DEBUG 
// Implementation of msgHeaderType peekMsgHeader (int socket) 	

int sendAll(int socket, const void *buffer, size_t length){
    const char *ptr = (const char *)buffer;
    size_t remaining = length;

    while (remaining > 0) {
        ssize_t sent = send(socket, ptr, remaining, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue; // Interrupted by signal, retry
            }
            (void)fprintf(stderr, "sendAll: %s\n", strerror(errno));
            return -1;
        }
        if (sent == 0) {
            (void)fprintf(stderr, "sendAll: connection closed\n");
            return -1;
        }
        ptr += (size_t)sent;
        remaining -= (size_t)sent;
    }
    return 0;
}

int receiveAll(int socket, void* buffer, size_t length) {
    char* ptr = (char*)buffer;
    size_t remaining = length;

    while (remaining > 0) {
        ssize_t received = recv(socket, ptr, remaining, 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue; // Interrupted by signal, retry
            }
            (void)fprintf(stderr, "receiveAll: %s\n", strerror(errno));
            return -1;
        }
        if (received == 0) {
            // Connection closed by the other end
            return -1;
        }
        ptr += (size_t)received;
        remaining -= (size_t)received;
    }
    return 0;
}

int send_all(int socket, const void *buffer, size_t length)
{
    return sendAll(socket, buffer, length);
}

int recv_all(int socket, void *buffer, size_t length)
{
    return receiveAll(socket, buffer, length);
}

messageHeaderType peekMessageHeader(int socket)
{
    messageHeaderType header;
    (void)memset(&header, 0, sizeof(header));

    if (receiveAll(socket, &header, sizeof(header)) < 0)
    {
        header.clientID = (uint32_t)-1;
    }
    if (header.clientID == (uint32_t)-1)
    {
        header.opID = OPR_BYE;
    }

    return header;
}

messageHeaderType peekmessageHeader(int socket)
{
    return peekMessageHeader(socket);
}