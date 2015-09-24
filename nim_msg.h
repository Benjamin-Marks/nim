#ifndef netinetin
#define netinetin
#include <netinet/in.h>
#endif

/*
 * The Game Structure
 * gameID represents the number game on the server. If this value is 
 * 0, there is one player waiting for a partner. In this case, HandleB
 * will be null. The value of non-zero gameIDs does not signify anything
 * meaningful, and is not adjusted based on the number of games in progress
 * handleA and handleB represent the usernames chosen by the two players
 */
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
    
