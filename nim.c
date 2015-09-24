#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "nim_msg.h"

void validateParams(int argc, const char* argv[]);
void usage(const char* name, int doExit);
int findServer();
void playGame(int sock);
int getBoard(int sock);
void printBoard(int board);
int startGame();
int validateMove(char* buf, int board);
void winGame(int win); //1 = win, 0 = loss

const char* password = NULL;
int usePass = 0;
int useQuery = 0;
int mySock = 0;
char* hostname = NULL;
char* tcpPort = NULL;
char* dgramPort = NULL;
char myHandle[21];
char oppHandle[21];

//TODO: Pressing enter waiting for query, etc. causes weird crashes

int main(int argc, const char* argv[]) {
    validateParams(argc, argv);
    if (!findServer()) {
        fprintf(stderr, "Warning: nim unable to find server file, waiting and retrying\n");
        sleep(60);
        if (!findServer()) {
            fprintf(stderr, "ERROR: Unable to find server info file\n");
            exit(0);
        }
    }
    if (useQuery) {
        char theQuery[200];
        if(queryServer(theQuery, 200, hostname, dgramPort)) {
            printf("%s\n", theQuery);
        } else {
            fprintf(stderr, "ERROR: No server response to query\n");
        }
        exit(0);
    } else {
        printf("Your Handle: ");
        fflush(stdout);        
        memset(myHandle, 0, sizeof(myHandle));
        fgets(myHandle, sizeof(myHandle), stdin); //TODO: Doesn't test for excess input?
        if (strchr(myHandle, ';') != NULL) {
            fprintf(stderr, "ERROR: ';' char invalid for handles\n");
            exit(0);
        }
        char* pos;
        if ((pos = strchr(myHandle, '\n')) != NULL) {
            *pos = '\0';
        }

        //connect with server to start game
        int fd, left, num, put, ecode;
        struct addrinfo hints, *addrlist;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_NUMERICSERV;
        hints.ai_protocol = 0;
        hints.ai_canonname = NULL;
        hints.ai_addr = NULL;
        hints.ai_next = NULL;

        ecode = getaddrinfo(hostname, tcpPort, &hints, &addrlist);
        if (ecode != 0) {
            fprintf(stderr, "startGame: %s\n", gai_strerror(ecode));
            exit(0);
        }

        if ((fd = socket(addrlist->ai_family, addrlist->ai_socktype, 0)) < 0) {
            perror("startGame:socket");
            exit(0);
        }

        if ( connect(fd, addrlist->ai_addr, addrlist->ai_addrlen) < 0) {
            perror("startGame:connect");
            exit(0);
        }

        //Send our handle with connection
        left = strlen(myHandle);
        put = 0;
        char c = 'H'; //Designates message as handle
        write(fd, &c, 1);
        while (left > 0) {
            if ((num = write(fd, myHandle + put, left)) < 0) {
                perror("startGame:write");
                exit(0);
            } else {
                left -= num;
            }
            put += num;
        }
        c = ';';
        write (fd, &c, 1);
        char type = '0';
        read(fd, &type, 1);
        if(type == 'E') { //No games in progress, wait
            printf("No Currently Available Players, please wait\n");
            read(fd, &type, 1); //Wait for the handle header 
        }
        printf("Retrieving partner information\n");

        char buffer[200];
        memset(buffer, 0, 200);
        char *ptr = buffer;
        while(read(fd, ptr, 1) == 1 && *ptr != ';') {
            ptr++;
        }
        //TODO: Should verify we're receiving handle message
        if (strlen(buffer) == 0) {
            fprintf(stderr, "Error: did not receive valid handle\n");
            exit(0);
        }
        memcpy(oppHandle, buffer, strlen(buffer) - 1);      
        playGame(fd);
    }
    return 0;
}

void playGame(int sock) { while(1) {
    int board = getBoard(sock);
    printBoard(board);
    //get move
    int invalid = 1;
    int move = 0;
    while (invalid) {
        printf("Your Move: ");
        char buf[1024];
        memset(buf, 0, 1024);
        fgets(buf, 1024, stdin);
        if((move = validateMove(buf, board)) != 0) {
            invalid = 0;
        } else {
            printf("Invalid Move. Format X Y where x is row and y is num to remove\n");
        }
    }
    //send move to server
    char c = 'M';
    write(sock, &c, 1);
    char moveChar[3];
    sprintf(moveChar, "%d", move);
    int left = strlen(moveChar);
    int put = 0;
    int num;
    while (left > 0) {
    if ((num = write(sock, moveChar + put, left)) < 0) {
        perror("startGame:write");
        exit(0);
    } else {
        left -= num;
    }
    put += num;
    }
    c = ';';
    write(sock, &c, 1);
}}

//gets board config from server
int getBoard(int sock) {
    char buffer[20];
    memset(buffer, 0, 20);
    int num = 0;
    while(read(sock, buffer + num, 1) == 1 && buffer[num] != ';' && buffer[num] != 'W' && buffer[num] != 'L') {
        num++;
    }
    if(buffer[0] != 'B') {
        if(buffer[0] == 'W') {
            winGame(1);
        } else if (buffer[0] == 'L') {
            winGame(0);
        } else {
            fprintf(stderr, "ERROR: server did not reply with board\n");
            exit(0);
        }
    }         
    return strtol(buffer + 1, NULL, 10);
}

//Prints the formatted board
void printBoard(int board) {
    //Get rows
    int rows[4];
    int i;
    for (i = 0; i < 4; i++) {
        rows[3-i] = board % 10;
        board /= 10;
    }
    printf("Game with %s\n", oppHandle);
    char text[15] = " 0 0 0 0 0 0 0";
    printf("Board:\n 1|%.*s\n 2|%.*s\n 3|%.*s\n 4|%.*s\n  +--------------------\n    1 2 3 4 5 6 7\n", rows[0] * 2, text, rows[1] * 2, text, rows[2] * 2, text, rows[3] * 2, text);
}

//Boolean method - Finds server file, loads IP address and port
//returns true if found valid server file, false otherwise
int findServer() {
    //File Location: this file's directory, title is serverinfo.txt
    errno = 0;
    FILE* serverLoc = fopen("serverinfo.txt", "r");
    if(serverLoc != NULL && errno == 0) { 
        char buffer[50];
        fgets(buffer, 50, serverLoc);
        fclose(serverLoc);
        
        char *startDir = buffer;
        char *endDir = strchr(startDir, ' ');
        int paramSize = endDir - startDir;
        hostname = malloc(paramSize + 1);
        memset(hostname, 0, paramSize + 1);
        memcpy(hostname, startDir, paramSize);
        
        startDir = ++endDir;
        endDir = strchr(startDir, ' ');
        paramSize = endDir - startDir;
        tcpPort = malloc(paramSize + 1);
        memcpy(tcpPort, startDir, paramSize);
        
        startDir=++endDir;       
        endDir = strchr(startDir, ' ');
        paramSize = endDir - startDir;
        dgramPort = malloc(paramSize + 1);
        memcpy(dgramPort, startDir, paramSize);
        return 1;
    }
    return 0;
}

//Verifies comand-line parameters, quits if invalid
void validateParams(int argc, const char* argv[]) {
    if (argc > 4) {
        fprintf(stderr, "ERROR: Too Many arguments\n");
        usage(argv[0], 1);
    }
    int curParam = 1;
    //Check validity of params
    while (argc > curParam) {
        if(!strcmp(argv[curParam], "-q")) {
            useQuery = 1;
            curParam++;
        } else if (!strcmp(argv[curParam], "-p")) {
            if (curParam + 1 >= argc) {
                fprintf(stderr, "ERROR: password must be supplied\n");
                usage(argv[0], 1);
            }
            usePass = 1;
            password = argv[++curParam];
            curParam++;
        } else {
            fprintf(stderr, "ERROR: invalid parameter submitted\n");
            usage(argv[0], 1);
        }
    }
}


//Outputs program usage and exits if requested
void usage(const char* name, int doExit) {
    printf("Usage: %s [-q] [-p XXXX]\n"
            "-q: query mode, will display current games on server\n"
            "-p XXXX: password mode, use XXXX password to login to server\n",
            name);
    if(doExit) {
        exit(0);
    }
}

//Returns move integer on success, 0 on failure
int validateMove(char* buf, int board) {
    char *endPtr;
    errno = 0;
    int row = strtol(buf, &endPtr, 10);
    if (endPtr == buf || errno != 0 || row > 4 || row < 1) {
        return 0;
    }
    errno = 0;
    int remove = strtol(endPtr, NULL, 10);
    if (errno != 0 || remove > 7 || remove < 1) {
        return 0;
    }

    //Determine if num to remove is valid
    int i;
    for (i = 4 - row; i > 0; i--) {
        board /= 10;
    }

    if (remove > board % 10) {
        return 0;
    }
    else {
        return row * 10 + remove;
    }
}

//Prints win/loss message and exits
void winGame(int win) {
    if (win) {
        printf("Your opponent took the last move or forfeited, you won!\n");
    } else {
        printf("You took the last move, you lost!\n");
    }
    exit(0);
}
