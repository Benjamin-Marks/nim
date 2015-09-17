#ifndef netinetin
#define netinetin
#include <netinet/in.h>
#endif


typedef struct game{
    int gameID; //if 0, handleB is null, no game in progress
    char handleA[21];
    char handleB[21];
    struct game *next;
    struct game *prev;
    }game;

int queryServer(char* buf, int bufSize, char* IP, char* port);
int retQuery(int dgramSock, game *curGame, struct sockaddr_in from);
struct sockaddr_in* getnewAddr(int dgram, int *sock);
    
