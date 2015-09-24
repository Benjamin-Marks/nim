#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifndef netinetin
#define netinetin
#include <netinet/in.h>
#endif
#include "nim_msg.h"


void sigUsrHandle();
void sigChildHandle();
void makeLocFile();
void runDaemon();


char hostname[128];
char IP[INET_ADDRSTRLEN];
int tcpSock = 0;
int dgramSock = 0;
int dgramPort = 0;
int tcpPort = 0;
game *games = NULL; //Linked list for games. Stack-style adding
struct sockaddr_in* waiter; //Info for computer waiting for partner

int main(int argc, const char* argv[]) {
    signal(SIGUSR2, sigUsrHandle);
    signal(SIGCHLD, sigChildHandle);

    //Create our sockets, get IP and ports
    struct sockaddr_in *sin = getnewAddr(0, &tcpSock);
    tcpPort = ntohs((unsigned short)(sin->sin_port));

    inet_ntop(AF_INET, &(sin->sin_addr.s_addr), IP, sizeof(IP));
    
    sin = getnewAddr(1, &dgramSock);
    dgramPort = ntohs((unsigned short)(sin->sin_port));

    //Get our host name
    if (gethostname(hostname, 128) != 0) {
        perror("nim_server:gethostname");
        exit(1);
    }
    //Make the serverinfo file for other processes to access
    makeLocFile();
    
    //Initialize Server Daemon
    runDaemon();
    printf("Shutting down nim_server\n");
    remove("serverinfo.txt");
    return 0;
}

void runDaemon() { while(1) { 
    char msg[22];    
    int connA;
    struct sockaddr_in from;
    socklen_t fsize = sizeof(from);
    fd_set mask;
    int hits, cc;
    struct timeval timeout;

    FD_ZERO(&mask);
    FD_SET(tcpSock, &mask);
    FD_SET(dgramSock, &mask);
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;

    printf("gonna select\n");
    if ((hits = select(dgramSock+1, &mask, (fd_set *)0, (fd_set *)0, &timeout)) < 0) {
        if (errno != EINTR) { //not from Child who died
        perror("nim_server:select");
        break;
        } else {
            FD_ZERO(&mask);
        }
    }
    printf("have selected\n");
    //FD_ISSET(hits) returns the int of the socket 
    if (FD_ISSET(dgramSock, &mask)) {
        printf("got dgram\n");
        cc = recvfrom(dgramSock, &msg, sizeof(msg), 0, (struct sockaddr *) &from, &fsize);
        if (cc < 0) {
            perror("nim_server:recvfromDGram");
            break;
        }
        //Validate message is a Query
        if(msg[0] == 'Q') {
            //Received query, return games
            retQuery(dgramSock, games, from);
        } else {
            fprintf(stderr, "nim_server:unrecognized message: %s\n", msg);
            break;
        }
    } else if (FD_ISSET(tcpSock, &mask)) {
        printf("got tcp\n");
        int conn;
        if((conn=accept(tcpSock, (struct sockaddr *)&from, &fsize)) < 0) {
            perror("nim_server:tcp:accept");
            break;
        }
        //Read Message
        char buffer[200];
        memset(buffer, 0, 200);
        char *ptr = buffer;
        while((read(conn, ptr, 1) == 1) && (*ptr != ';')) {
            ptr++;
        }
        if (games == NULL || games->gameID != 0) {
            //Return: "No games availble -> Empty msg
            connA = conn;
            char c = 'E';
            write(conn, &c, 1);
            //Update Games
            game *newGame = malloc(sizeof(game));
            newGame->gameID = 0;
            memcpy(newGame->handleA, buffer + 1, strlen(buffer) - 2); //Remove head & tail
            newGame->next = games;
            if (games != NULL) {
                games->prev = newGame;
            }
            games = newGame;
        } else {
            //Update Games
            memcpy(games->handleB, buffer + 1, strlen(buffer) - 2);
            //Send connection info to MatchServer and fork
            int childPID = fork();
            if (!childPID) { //match_server
                char sconnA[3];
                sprintf(sconnA, "%d", connA);
                char sconnB[3];
                sprintf(sconnB, "%d", conn);
                char* theArgs[] = {"nim_match_server", games->handleA, games->handleB, sconnA, sconnB, NULL};
                char* theEnv[] = {NULL};
                execve("nim_match_server", theArgs, theEnv);
            } else { //Still server
                games->gameID = childPID; 
            }
        }
    } else {
        printf("got nothing\n");
    }
}}


//catches SIGUSR2, shuts down server
void sigUsrHandle() {
    //Delete server file and exit
    remove("serverinfo.txt");
    exit(0);
}

void sigChildHandle() {
    //Find the dead child, dispose of it
    pid_t pid;
    int status;
    pid = waitpid(-1, &status, WNOHANG);
    if(!WIFEXITED(status) && !WIFSIGNALED(status)) {
        return;
    }
    //search games for pid
    if (games == NULL) {
        return;
    }
    game *curGame = games;
    while(curGame != NULL) {
        if (curGame->gameID == pid) {
            if (games->gameID == curGame->gameID) { //First kid
                if (games->next != NULL) {
                    games->next->prev = NULL;
                }
                games = games->next;
            } else {
                game* temp = curGame->prev;
                curGame->prev->next = curGame->next;
                if (curGame->next != NULL) {
                    curGame->next->prev = temp;
                }
            }
            return;
        }
        curGame = curGame->next;
    }
}


//Prints out hostname and port numbers for the server from global variables
//Pre-Conditions: hostname not null
void makeLocFile() {
    FILE* locFile = fopen("serverinfo.txt", "w");
    fprintf(locFile, "%s %d %d \n", hostname, tcpPort, dgramPort);
    fclose(locFile);
}
