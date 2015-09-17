#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nim_msg.h"

void gameLoop();
int switchPlayer(int curPlayer);
int updateBoard(int board, char* buf);
void winGame(int curPlayer);

//argv[1] = HandleA
//argv[2] = HandleB
//argv[3] = connA
//argv[4] = connB

const char* handleA;
const char* handleB;
int connA;
int connB;


int main(int argc, const char* argv[]) {
    if (argc != 5) {
        fprintf(stderr, "nim_match_server invoked incorrectly %d\n", argc);
        exit(0);
    }
    handleA = argv[1];
    handleB = argv[2];
    //Create our Connections
    connA = strtol(argv[3], NULL, 10);
    connB = strtol(argv[4], NULL, 10);

    //Send playerA opponent handle
    int num;
    int left = strlen(handleB);
    int put = 0;
    char c = 'H'; //Designates message as handle
    write(connA, &c, 1);
    while (left > 0) {
    if ((num = write(connA, handleB + put, left)) < 0) {
        perror("match_server:startGame:write");
        exit(0);
    } else {
        left -= num;
    }
    put += num;
    }
    c = ';';
    write(connA, &c, 1);

    //Send playerB oponent handle 
    left = strlen(handleA);
    put = 0;
    c = 'H'; //Designates message as handle
    write(connB, &c, 1);
    while (left > 0) {
    if ((num = write(connB, handleA + put, left)) < 0) {
        perror("match_server:startGame:write");
        exit(0);
    } else {
        left -= num;
    }
    put += num;
    }
    c = ';';
    write(connB, &c, 1);
    
    //And fuck it we're done. See if this works
    gameLoop();

    return 0;
}

void gameLoop() { 
    int curPlayer = connA; //socket for the current player
    int board = 91357;  //Each digit is the number of pegs in the corresponding row, 9 added to front
   while(1) { //Game in Progress
        //send Board to player
        char c = 'B';
        write(curPlayer, &c, 1);
        char boardChar[6];
        sprintf(boardChar, "%d", board);
        int left = strlen(boardChar);
        int put = 0;
        int num;
        while (left > 0) {
        if ((num = write(curPlayer, boardChar + put, left)) < 0) {
            perror("nim_server:write");
            exit(0);
        } else {
            left -= num;
        }
        put += num;
        }
        c = ';';
        write(curPlayer, &c, 1);
        //Read player's move
        char buffer[3];
        c = '0';
        read(curPlayer, &c, 1); //Get Type
        if (c != 'M') {
            fprintf(stderr, "nim_match:invalid message type %c\n", c);
            winGame(switchPlayer(curPlayer));
            }
        read(curPlayer, buffer, 1); //read the move TODO: error check this stuff?
        read(curPlayer, buffer + 1, 1);
        read(curPlayer, &c, 1);
        if (c != ';') {
            fprintf(stderr, "nim_match:invalid move\n");
            winGame(switchPlayer(curPlayer));          
        }
        board = updateBoard(board, buffer);
        if (board == 90000) {
            winGame(switchPlayer(curPlayer));
        }
        curPlayer = switchPlayer(curPlayer);    
    }
}

//Switches curPlayer
int switchPlayer(int curPlayer) {
    if (curPlayer == connA) {
        return connB;
    } else {
        return connA;
    }
}

//Updates board
//Buf: unparsed chars for move
int updateBoard(int board, char* buf) {
    int row = strtol(buf, NULL, 10);
    int remove = row % 10;
    row /= 10;
    printf("move: %d %d\n", row, remove);
    int i;
    for (i = 4-row; i > 0; i--) {
        remove *= 10;
    }
    return board - remove;
}

void winGame(int curPlayer) {
    char w = 'W';
    char l = 'L';
    if (curPlayer == connA) {
        write(connA, &w, 1);
        write(connB, &l, 1);
    } else {
        write(connA, &l, 1);
        write(connB, &w, 1);
    }
    exit(0);
}
