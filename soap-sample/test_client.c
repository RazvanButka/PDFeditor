/**
 * Razvan Butka
 * IR3 2026, group 3
 * SOAP Test Client for PDFeditor
 * 
 * The program below acts as a SOAP test client. It connects through TCP sockets
 * to a local SOAP server running on port 18082 and manually builds HTTP requests
 * that contain SOAP envelopes. The program sends multiple test operations to the
 * server, receives the responses, and prints them to STDOUT.
 *
 * I handled the following edge cases that may occur:
 * -- socket creation failure
 * -- connection failure to the server
 * -- sending request failure
 * -- safe receiving of the server response into buffer
 */

#include <stdio.h>      /* used for: printf, snprintf, perror */
#include <stdlib.h>     /* used for: exit related operations */
#include <string.h>     /* used for: strlen, memset, snprintf */
#include <unistd.h>     /* used for: close */
#include <sys/socket.h> /* used for: socket, connect, send, recv */
#include <netinet/in.h> /* used for: sockaddr_in structure */
#include <arpa/inet.h>  /* used for: inet_pton, htons */

/**
 * test_operation - builds and sends a SOAP request for a specific operation
 * @operation: name of the SOAP operation to call
 * @param: optional parameter to pass to the operation (NULL if not needed)
 *
 * This function creates a TCP socket, connects to the SOAP server on localhost:18082,
 * manually constructs an HTTP request containing a SOAP envelope, sends it, and
 * receives the response which is printed to stdout.
 */
void test_operation(const char *operation, const char *param) {
    int sock;
    struct sockaddr_in addr;
    char response[4096];
    int bytes;

    /* build SOAP body depending on whether a parameter is provided */
    char soap_body[1024];
    if (param) {
        snprintf(soap_body, sizeof(soap_body),
            "<ns1:%s>\n<in>%s</in>\n</ns1:%s>",
            operation, param, operation);
    } else {
        snprintf(soap_body, sizeof(soap_body),
            "<ns1:%s></ns1:%s>",
            operation, operation);
    }

    /* build complete SOAP envelope with XML declaration and headers */
    char body[2048];
    snprintf(body, sizeof(body),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
        "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "xmlns:ns1=\"http://www.example.org/operations/\">\r\n"
        "<SOAP-ENV:Body>\r\n"
        "%s\r\n"
        "</SOAP-ENV:Body>\r\n"
        "</SOAP-ENV:Envelope>",
        soap_body);

    /* build final HTTP request with correct Content-Length header */
    char final_request[4096];
    snprintf(final_request, sizeof(final_request),
        "POST / HTTP/1.1\r\n"
        "Host: localhost:18082\r\n"
        "Content-Type: text/xml; charset=UTF-8\r\n"
        "SOAPAction: http://www.example.org/operations/%s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        operation, strlen(body), body);

    /* print operation information to stdout */
    printf("=== Testing: %s", operation);
    if (param) printf(" (%s)", param);
    printf(" ===\n");

    /* create TCP socket for communication */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return;
    }

    /* set server address information for connection */
    addr.sin_family = AF_INET;
    addr.sin_port = htons(18082);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    /* connect to the SOAP server on localhost */
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return;
    }

    /* send the SOAP HTTP request to the server */
    if (send(sock, final_request, strlen(final_request), 0) < 0) {
        perror("send");
        close(sock);
        return;
    }

    /* receive the server response into buffer safely */
    memset(response, 0, sizeof(response));
    while ((bytes = recv(sock,
                         response + strlen(response),
                         sizeof(response) - strlen(response) - 1, 0)) > 0) {
        response[strlen(response) + bytes] = '\0';
    }

    /* close socket after communication is complete */
    close(sock);

    /* print the response received from server */
    printf("%s\n\n", response);
}

/**
 * main - entry point for the SOAP test client
 *
 * Executes a series of test operations on the SOAP server to validate
 * that it is running correctly and responding to requests.
 *
 * Return: 0 on successful execution
 */
int main() {
    printf("\n=== PDFeditor SOAP Test Client ===\n\n");

    /* test multiple SOAP operations exposed by the server */
    test_operation("createFile", "test1.txt");
    test_operation("createFile", "test2.txt");
    test_operation("servFiles", NULL);
    test_operation("filesStatistics", NULL);
    test_operation("filesInCertainProgLanguage", "c");

    return 0;
}