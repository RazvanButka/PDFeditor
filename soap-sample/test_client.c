#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void test_operation(const char *operation, const char *param) {
    int sock;
    struct sockaddr_in addr;
    char request[2048];
    char response[4096];
    int bytes;
    
    // Build SOAP request body
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
    
    // Build HTTP request with SOAP envelope
    snprintf(request, sizeof(request),
        "POST / HTTP/1.1\r\n"
        "Host: localhost:18082\r\n"
        "Content-Type: text/xml; charset=UTF-8\r\n"
        "SOAPAction: http://www.example.org/operations/%s\r\n"
        "Content-Length: %%d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
        "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "xmlns:ns1=\"http://www.example.org/operations/\">\r\n"
        "<SOAP-ENV:Body>\r\n"
        "%s\r\n"
        "</SOAP-ENV:Body>\r\n"
        "</SOAP-ENV:Envelope>",
        operation, soap_body);
    
    // Calculate body size
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
    
    // Final request with correct content length
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
    
    printf("=== Testing: %s", operation);
    if (param) printf(" (%s)", param);
    printf(" ===\n");
    
    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return;
    }
    
    // Connect to server
    addr.sin_family = AF_INET;
    addr.sin_port = htons(18082);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return;
    }
    
    // Send request
    if (send(sock, final_request, strlen(final_request), 0) < 0) {
        perror("send");
        close(sock);
        return;
    }
    
    // Receive response
    memset(response, 0, sizeof(response));
    while ((bytes = recv(sock, response + strlen(response), 
                        sizeof(response) - strlen(response) - 1, 0)) > 0) {
        response[strlen(response) + bytes] = '\0';
    }
    close(sock);
    
    // Print response
    printf("%s\n\n", response);
}

int main() {
    printf("\n=== PDFeditor SOAP Test Client ===\n\n");
    
    // Test operations
    test_operation("createFile", "test1.txt");
    test_operation("createFile", "test2.txt");
    test_operation("servFiles", NULL);
    test_operation("filesStatistics", NULL);
    test_operation("filesInCertainProgLanguage", "c");
    
    return 0;
}
