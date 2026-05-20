#define _POSIX_C_SOURCE 200809L

#include "proto.h"

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <unistd.h>

#define DEFAULT_UNIX_PATH "/tmp/admin.sock"
#define RESULT_BUFFER_SIZE 8192

static int gSocket = -1;
static uint32_t gClientID = 0;

static WINDOW* gResultWin = NULL;
static WINDOW* gMenuWin = NULL;
static WINDOW* gHeaderWin = NULL;
static WINDOW* gInputWin = NULL;

static void initWindows(void){
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    
    if(has_colors()){
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLUE);
        init_pair(2, COLOR_BLACK, COLOR_CYAN);
        init_pair(3, COLOR_WHITE, COLOR_BLACK);
        init_pair(4, COLOR_YELLOW, COLOR_BLACK);
        init_pair(5, COLOR_GREEN, COLOR_BLACK);
        init_pair(6, COLOR_RED, COLOR_BLACK);
        init_pair(7, COLOR_CYAN, COLOR_BLACK);
    }

    int rows, columns;
    getmaxyx(stdscr, rows, columns);

    gHeaderWin = newwin(3, columns, 0, 0);
    wbkgd(gHeaderWin, COLOR_PAIR(1));

    gMenuWin = newwin(rows - 5, 32, 3, 0);
    box(gMenuWin, 0, 0);

    gResultWin = newwin(rows - 5, columns - 32, 3, 32);
    box(gResultWin, 0, 0);
    scrollok(gResultWin, TRUE);

    gInputWin = newwin(2, columns, rows - 2, 0);
    wbkgd(gInputWin, COLOR_PAIR(7));
}

static void destroyWindows(void){
    if(gHeaderWin) delwin(gHeaderWin);
    if(gMenuWin) delwin(gMenuWin);
    if(gResultWin) delwin(gResultWin);
    if(gInputWin) delwin(gInputWin);
    endwin();
}

static void drawHeader(void){
    werase(gHeaderWin);
    wattron(gHeaderWin, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(gHeaderWin, 1, 2, "PDF Admin Interface - Client ID: %u", gClientID);
    wattroff(gHeaderWin, COLOR_PAIR(1) | A_BOLD);
    wrefresh(gHeaderWin);
}   

static void drawMenu(int selectedMenuItem){
    static const char* menuItems[] = {
        "1. Client list",
        "2. Job list",
        "3. System statistics",
        "4. Execution medium duration",
        "5. Job execution history",
        "6. Kick client",
        "7. Kill job",
        "8. IP block",
        "9. IP unblock",
        "q. Exit",
        NULL
    };
    werase(gMenuWin);
    box(gMenuWin, 0, 0);
    wattron(gMenuWin, COLOR_PAIR(4) | A_BOLD);
    mvwprintw(gMenuWin, 1, 2, "Commands for admin:");
    wattroff(gMenuWin, COLOR_PAIR(4) | A_BOLD);

    for(int i = 0; menuItems[i] != NULL; i++){
        if(i == selectedMenuItem){
            wattron(gMenuWin, COLOR_PAIR(2) | A_BOLD);
            mvwprintw(gMenuWin, i + 3, 2, "%-28s", menuItems[i]);
            wattroff(gMenuWin, COLOR_PAIR(2) | A_BOLD);
        } else {
            wattron(gMenuWin, COLOR_PAIR(3));
            mvwprintw(gMenuWin, i + 3, 2, "%-28s", menuItems[i]);
            wattroff(gMenuWin, COLOR_PAIR(3));
        }
    }
    wrefresh(gMenuWin);
}

static void resultClear(void){
    werase(gResultWin);
    box(gResultWin, 0, 0);
    wrefresh(gResultWin);
}

static void resultPrint(const char* text, int colorPair){
    werase(gResultWin);
    box(gResultWin, 0, 0);
    wattron(gResultWin, COLOR_PAIR(colorPair));
    mvwprintw(gResultWin, 1, 2, "%s", text);
    wattroff(gResultWin, COLOR_PAIR(colorPair));
    wrefresh(gResultWin);
}

static void inputPrompt(const char* prompt, char* buffer, size_t bufferSize){
    werase(gInputWin);
    wattron(gInputWin, COLOR_PAIR(7) | A_BOLD);
    mvwprintw(gInputWin, 0, 2, "%s", prompt);
    wattroff(gInputWin, COLOR_PAIR(7) | A_BOLD);
    wrefresh(gInputWin);

    echo();
    curs_set(1);
    wgetnstr(gInputWin, buffer, (int)bufferSize - 1);
    noecho();
    curs_set(0);

    werase(gInputWin);
    wrefresh(gInputWin);
}

static void inputStatus(const char* statusMessage, int isError){
    werase(gInputWin);
    wattron(gInputWin, COLOR_PAIR(isError ? 6 : 5) | A_BOLD);
    mvwprintw(gInputWin, 0, 2, "%s", statusMessage);
    wattroff(gInputWin, COLOR_PAIR(isError ? 6 : 5) | A_BOLD);
    wrefresh(gInputWin);
    wgetch(gInputWin);
    werase(gInputWin);
    wrefresh(gInputWin);
}

static int sendSimpleOperation(uint32_t opID){
    messageHeaderType header;
    (void)memset(&header, 0, sizeof(header));
    header.messageSize = htonl((uint32_t)sizeof(header));
    header.clientID = htonl(gClientID);
    header.opID = htonl(opID);
    header.statusCode = htonl(STATUS_OK);
    return send_all(gSocket, &header, sizeof(header));
}

static int receiveTestResponse(char* outputBuffer, size_t outputSize){
    messageHeaderType header;
    if(recv_all(gSocket, &header, sizeof(header)) < 0){
        return -1;
    }
    uint32_t status = ntohl(header.statusCode);
    if(status != STATUS_OK){
        (void)snprintf(outputBuffer, outputSize, "Operation failed with status code: %u", status);
        return 0;
    }
    uint32_t payloadLengthNet;
    if(recv_all(gSocket, &payloadLengthNet, sizeof(payloadLengthNet)) < 0){
        return -1;
    }
    uint32_t payloadLength = ntohl(payloadLengthNet);
    if(payloadLengthNet == 0){
        (void)strncpy(outputBuffer, "Operation completed successfully. No additional data.", outputSize - 1);
        return 0;
    }
    size_t toRead = (payloadLength < outputSize - 1) ? (size_t)payloadLength : outputSize - 1;
    if(recv_all(gSocket, outputBuffer, toRead) < 0){
        return -1;
    }
    outputBuffer[toRead] = '\0';

    size_t extra = (size_t)payloadLength - toRead;
    char discard[1024];
    while(extra > 0){
        size_t chunk = (extra < sizeof(discard)) ? extra : sizeof(discard);
        if(recv_all(gSocket, discard, chunk) < 0){
            return -1;
        }
        extra -= chunk;
    }
    return 0;
}

static void actionListClients(void){
    resultClear();
    char resultBuffer[RESULT_BUFFER_SIZE];
    if(sendSimpleOperation(OPR_ADMIN_LIST_CLIENTS) < 0 || receiveTestResponse(resultBuffer, sizeof(resultBuffer)) < 0){
        resultPrint("Failed to send request to server.", 6);
        return;
    }
    resultPrint(resultBuffer, 3);
}

static void actionListJobs(void){
    resultClear();
    char resultBuffer[RESULT_BUFFER_SIZE];
    if(sendSimpleOperation(OPR_ADMIN_LIST_JOBS) < 0 || receiveTestResponse(resultBuffer, sizeof(resultBuffer)) < 0){
        resultPrint("Failed to send request to server.", 6);
        return;
    }
    resultPrint(resultBuffer, 3);
}

static void actionSystemStats(void){
    resultClear();
    char resultBuffer[RESULT_BUFFER_SIZE];
    if(sendSimpleOperation(OPR_ADMIN_SYS_STATS) < 0 || receiveTestResponse(resultBuffer, sizeof(resultBuffer)) < 0){
        resultPrint("Failed to send request to server.", 6);
        return;
    }
    resultPrint(resultBuffer, 5);
}

static void actionAverageExecution(void){
    resultClear();
    char resultBuffer[RESULT_BUFFER_SIZE];
    if(sendSimpleOperation(OPR_ADMIN_AVG_EXEC_TIME) < 0 || receiveTestResponse(resultBuffer, sizeof(resultBuffer)) < 0){
        resultPrint("Failed to send request to server.", 6);
        return;
    }
    resultPrint(resultBuffer, 5);
}

static void actionJobHistory(void){
    resultClear();
    char resultBuffer[RESULT_BUFFER_SIZE];
    if(sendSimpleOperation(OPR_ADMIN_JOB_HISTORY) < 0 || receiveTestResponse(resultBuffer, sizeof(resultBuffer)) < 0){
        resultPrint("Failed to send request to server.", 6);
        return;
    }
    resultPrint(resultBuffer, 3);
}

static void actionKickClient(void){
    char inputBuffer[64];
    inputPrompt("Enter client ID to kick: ", inputBuffer, sizeof(inputBuffer));
    uint32_t clientID = (uint32_t)strtol(inputBuffer, NULL, 10);
    if(clientID <= 0){
        inputStatus("Invalid client ID.", 1);
        return;
    }
    adminKickmessageType message;
    (void)memset(&message, 0, sizeof(message));
    message.header.messageSize = htonl((uint32_t)sizeof(message));
    message.header.clientID = htonl(gClientID);
    message.header.opID = htonl(OPR_ADMIN_KICK_CLIENT);
    message.header.statusCode = htonl(STATUS_OK);
    message.targetClientID = htonl((uint32_t)clientID);
    if(send_all(gSocket, &message, sizeof(message)) < 0){
        inputStatus("Failed to send request to server.", 1);
        return;
    }
    messageHeaderType responseHeader;
    if(recv_all(gSocket, &responseHeader, sizeof(responseHeader)) < 0){
        inputStatus("Failed to receive response from server.", 1);
        return;
    }

    uint32_t status = ntohl(responseHeader.statusCode);
    if(status == STATUS_OK){
        inputStatus("Client kicked successfully.", 0);
        resultClear();
    } else if(status == STATUS_NOT_FOUND) {
        inputStatus("Client ID not found.", 1);
    } else if(status == STATUS_UNAUTHORIZED) {
        inputStatus("Cannot kick admin client.", 1);
    } else {
        inputStatus("Unknown error occurred.", 1);
    }
}

static void actionKillJob(void){
    char inputBuffer[64];
    inputPrompt("Enter job ID to kill: ", inputBuffer, sizeof(inputBuffer));
    uint32_t jobID = (uint32_t)strtol(inputBuffer, NULL, 10);
    if(jobID <= 0){
        inputStatus("Invalid job ID.", 1);
        return;
    }
    adminKillJobmessageType message;
    (void)memset(&message, 0, sizeof(message));
    message.header.messageSize = htonl((uint32_t)sizeof(message));
    message.header.clientID = htonl(gClientID);
    message.header.opID = htonl(OPR_ADMIN_KILL_JOB);
    message.header.statusCode = htonl(STATUS_OK);
    message.targetJobID = htonl((uint32_t)jobID);
    if(send_all(gSocket, &message, sizeof(message)) < 0){
        inputStatus("Failed to send request to server.", 1);
        return;
    }
    messageHeaderType responseHeader;
    if(recv_all(gSocket, &responseHeader, sizeof(responseHeader)) < 0){
        inputStatus("Failed to receive response from server.", 1);
        return;
    }
    uint32_t status = ntohl(responseHeader.statusCode);
    if(status == STATUS_OK){
        inputStatus("Job killed successfully.", 0);
        resultClear();
    } else if(status == STATUS_NOT_FOUND) {
        inputStatus("Job ID not found.", 1);
    } else {
        inputStatus("Unknown error occurred.", 1);
    }
}

static void actionIPBlock(int block){
    char inputBuffer[48];
    inputPrompt("Enter IP address to block: ", inputBuffer, sizeof(inputBuffer));
    if(strlen(inputBuffer) == 0){
        inputStatus("Invalid IP address.", 1);
        return;
    }
    adminIPmessageType message;
    (void)memset(&message, 0, sizeof(message));
    message.header.messageSize = htonl((uint32_t)sizeof(message));
    message.header.clientID = htonl(gClientID);
    message.header.opID = htonl(block ? OPR_ADMIN_BLOCK_IP : OPR_ADMIN_UNBLOCK_IP);
    message.header.statusCode = htonl(STATUS_OK);
    (void)strncpy(message.ipAddr, inputBuffer, sizeof(message.ipAddr) - 1);
    if(send_all(gSocket, &message, sizeof(message)) < 0){
        inputStatus("Failed to send request to server.", 1);
        return;
    }

    messageHeaderType responseHeader;
    if(recv_all(gSocket, &responseHeader, sizeof(responseHeader)) < 0){
        inputStatus("Failed to receive response from server.", 1);
        return;
    }
    uint32_t status = ntohl(responseHeader.statusCode);
    if(status == STATUS_OK){
        inputStatus(block ? "IP blocked successfully." : "IP unblocked successfully.", 0);
    } else if(status == STATUS_ALREADY_BLOCKED) {
        inputStatus("IP is already blocked.", 1);
    }
     else if(status == STATUS_NOT_FOUND) {
        inputStatus("IP not found on black list.", 1);
    } 
     else {
        inputStatus("Unknown error occurred.", 1);
    }
}

static int connectToServer(const char* unixPath){
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if(sock < 0){
        perror("socket");
        return -1;
    }
    struct sockaddr_un serverAddr;
    (void)memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;
    (void)strncpy(serverAddr.sun_path, unixPath, sizeof(serverAddr.sun_path) - 1);
    if(connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0){
        perror("connect UNIX");
        close(sock);
        return -1;
    }
    return sock;
}

static void runUI(void){
    static const int NUM_MENU_ITEMS = 9;
    int selectedMenuItem = 0;
    drawHeader();
    drawMenu(selectedMenuItem);
    resultClear();

    resultPrint("Welcome to PDF Admin Interface! Use the menu to perform administrative actions.", 5);

    int running = 1;
    while(running){
        int ch = wgetch(gMenuWin);
        switch(ch){
            case KEY_UP:
                selectedMenuItem = (selectedMenuItem > 0) ? (selectedMenuItem - 1) : (NUM_MENU_ITEMS - 1);
                break;
            case KEY_DOWN:
                selectedMenuItem = (selectedMenuItem < NUM_MENU_ITEMS - 1) ? (selectedMenuItem + 1) : 0;
                break;
            case '\n':
            case KEY_ENTER:
                switch(selectedMenuItem){
                    case 0: actionListClients(); break;
                    case 1: actionListJobs(); break;
                    case 2: actionSystemStats(); break;
                    case 3: actionAverageExecution(); break;
                    case 4: actionJobHistory(); break;
                    case 5: actionKickClient(); break;
                    case 6: actionKillJob(); break;
                    case 7: actionIPBlock(1); break;
                    case 8: actionIPBlock(0); break;
                }
                break;
            case '1':
                selectedMenuItem = 0;
                actionListClients();
                break;
            case '2':
                selectedMenuItem = 1;
                actionListJobs();
                break;
            case '3':
                selectedMenuItem = 2;
                actionSystemStats();
                break;
            case '4':
                selectedMenuItem = 3;
                actionAverageExecution();
                break;
            case '5':
                selectedMenuItem = 4;
                actionJobHistory();
                break;
            case '6':
                selectedMenuItem = 5;
                actionKickClient();
                break;
            case '7':
                selectedMenuItem = 6;
                actionKillJob();
                break;
            case '8':
                selectedMenuItem = 7;
                actionIPBlock(1);
                break;
            case '9':
                selectedMenuItem = 8;
                actionIPBlock(0);
                break;
            case 'q':
            case 'Q':
                running = 0;
                return;
            default:
                break;
        }
        drawHeader();
        drawMenu(selectedMenuItem);
    }
}

int main(int argc, char* argv[]){
    const char* unixPath = DEFAULT_UNIX_PATH;
    int opt;

    while((opt = getopt(argc, argv, "p:")) != -1){
        switch(opt){
            case 'p':
                unixPath = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s [-p unix_socket_path]\n", argv[0]);
                return EXIT_FAILURE;
        }
    }
    (void)fprintf(stderr, "Connecting to server at UNIX socket: %s\n", unixPath);

    gSocket = connectToServer(unixPath);
    if(gSocket < 0){
        (void)fprintf(stderr, "Failed to connect to server. Exiting.\n");
        return EXIT_FAILURE;
    }

    messageHeaderType hello;
    if(recv_all(gSocket, &hello, sizeof(hello)) < 0){
        (void)fprintf(stderr, "Failed to receive hello message from server. Exiting.\n");
        (void)close(gSocket);
        return EXIT_FAILURE;
    }
    gClientID = ntohl(hello.clientID);
    (void)fprintf(stderr, "Connected to server. Assigned client ID: %u\n", gClientID);

    initWindows();
    runUI();
    destroyWindows();

    messageHeaderType byebye;
    (void)memset(&byebye, 0, sizeof(byebye));
    byebye.messageSize = htonl((uint32_t)sizeof(byebye));
    byebye.clientID = htonl(gClientID);
    byebye.opID = htonl(OPR_BYE);
    (void)send_all(gSocket, &byebye, sizeof(byebye));
    (void)close(gSocket);

    (void)printf("Disconnected from server. Exiting.\n");
    return EXIT_SUCCESS;
}
