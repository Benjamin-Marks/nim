#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "nim_msg.h"

/* 
 * Message Codes: Head:
 *
 * Note: Queries do not have tails, some don't have heads
 *
 * B: Board config
 * C: Connect to start game, includes handle
 * E: Empty Message ***DOES NOT HAVE TAIL***
 * L: Game Over - Lost ***DOES NOT HAVE TAIL***
 * M: Move config
 * Q: Query (sent to server)
 * P: Query with password
 * W: Game Over - Won ***DOES NOT HAVE TAIL***
 * ';': Tail
 */

//TODO: combine queryServer and startGame code?

int queryServer(char* buf, int bufSize, char* hostname, char* port) {
    int fd, cc, ecode;
    struct addrinfo hints, *addrlist;
    struct sockaddr_in from;
    socklen_t fsize = sizeof(from);
    char msg[2] = "Q";
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    //construct remote socket address
    if ((ecode = getaddrinfo(hostname, port, &hints, &addrlist)) != 0) {
        fprintf(stderr, "ERROR:getaddrinfo: %s\n", gai_strerror(ecode));
        return 0;
    }

    //create socket
    fd = socket(addrlist->ai_family, addrlist->ai_socktype, addrlist->ai_protocol);
    if (fd == -1) {
        perror("queryserver:socket");
        return 0;
    }

    //Send datagram
    ecode = sendto(fd, &msg, sizeof(msg), 0, addrlist->ai_addr, addrlist->ai_addrlen);
    if(ecode == -1) {
        perror("queryServer:sendto");
        return 0;
    }
    fd_set mask;
    int hits;
    struct timeval timeout;
    FD_ZERO(&mask);
    FD_SET(0, &mask);
    FD_SET(fd, &mask);
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;

    if ((hits = select(fd+1, &mask, (fd_set *)0, (fd_set *)0, &timeout)) < 0) {
        perror("queryServer:select");
        exit(1);
    }

    if ((hits == 0) || ((hits > 0) && (FD_ISSET(0, &mask)))) {
        fprintf(stderr, "Server has timed out\n");
        return 0;
    }
    //Wait for Response - quit after 60 seconds of nothing
    //TODO: bufsize doesn't allow for unlimited games, only ~4, need loop
    char qryBuf[200];
    cc = recvfrom(fd, &qryBuf, sizeof(qryBuf), 0, (struct sockaddr *) &from, &fsize);
    if (cc < 0) {
        perror("nim_server:recvfromDGram");
    }

    //See if games exist
    if (qryBuf[0] == 'E') {
        //Size of return = 39 + 1 = 40
        int msgSize = 40;
        memcpy(buf, "There are no current games being played", msgSize);
    } else {
        printf("Current Matches:\n%s", qryBuf);
    }

    return 1;
}

int retQuery(int dgramSock, game *curGame, struct sockaddr_in from) {
    socklen_t fsize = sizeof(from);
    if(curGame == NULL) { //no games, send empty datagram
        char msg[2] = "E";
        int ecode = sendto(dgramSock, &msg, sizeof(msg), 0,
        (struct sockaddr *) &from, fsize);

        if (ecode == -1) {
            perror("retQuery:sendto");
            return 0;
        }
    } else {
        //handle + padding + any other info
        char msg[200] = "";
        char* startPtr = &msg[0];

        while(curGame != NULL) { 
            memcpy(startPtr, curGame->handleA, strlen(curGame->handleA));
            startPtr += strlen(curGame->handleA);
            if (curGame->gameID != 0) {//There's a handle B
                memcpy(startPtr, " vs ", 4);
                startPtr += 4;
                memcpy(startPtr, curGame->handleB, strlen(curGame->handleB));
                startPtr += strlen(curGame->handleB);
                memcpy(startPtr, "\n", 1);
                startPtr++;
            } else {
                memcpy(startPtr, "  ***Waiting For Partner***\n", 28);
                startPtr += 28;
            }
            curGame = curGame->next;
        }
        int ecode = sendto(dgramSock, &msg, strlen(msg), 0,
        (struct sockaddr *) &from, fsize);   

       if (ecode == -1) {
            perror("retQuery:sendto");
            return 0;
        }
        
    }
    return 1;
}

/*
 * Returns sockaddr_in struct for a new address
 * Parameters: dgram: boolean: 1 returns dgram socket, 0 returns tcp socket
 * sock: will be filled with socket number
 */
struct sockaddr_in* getnewAddr(int dgram, int* sock) {

    //Code from myaddrs.c
    int s;
    struct sockaddr_in *localaddr;
    int ecode;
    socklen_t length;
    struct addrinfo hints;
    struct addrinfo *addrlist;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    if (dgram) {
        hints.ai_socktype = SOCK_DGRAM;
    } else {
        hints.ai_socktype = SOCK_STREAM;
    }
    hints.ai_flags = AI_NUMERICSERV | AI_PASSIVE;
    hints.ai_protocol = 0;
    hints.ai_next = NULL;

    ecode = getaddrinfo(NULL, "0", &hints, &addrlist);
    if (ecode != 0) {  
        fprintf(stderr, "ERROR: getaddrinfo%s\n", gai_strerror(ecode));
        exit(1);
    }

    localaddr = (struct sockaddr_in *) addrlist->ai_addr;

    if ((s = socket(addrlist->ai_family, addrlist->ai_socktype, 0)) < 0) {
        perror("nim_server:socket");
        exit(1);
    }

    if (bind(s, (struct sockaddr *)localaddr, sizeof(struct sockaddr_in)) < 0) {
        perror("nim_server:bind");
        exit(1);
    }

    if (!dgram) {
        listen(s, 1);
    }
        

    length = sizeof(struct sockaddr_in);
    if (getsockname(s, (struct sockaddr *)localaddr, &length) < 0) {
        perror("nim_server:getsockname");
        exit(1);
    }

    *sock = s;

    return localaddr;
}
