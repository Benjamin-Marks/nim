#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nim_msg.h"

void gameLoop();
int switchPlayer(int curPlayer);
int updateBoard(int board, char* buf);
void winGame(int curPlayer);

const char* handleA;
const char* handleB;
int connA;
int connB;

/*
 * argv[1] = HandleA
 * argv[2] = HandleB
 * argv[3] = connA
 * argv[4] = connB
 */
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
    
    //Begin playing game
    gameLoop();

    return 0;
}

void gameLoop() { 
    int curPlayer = connA; //socket for the current player
    /*
     * Board represents the curent state of the pegs in the board
     * Each digit corresponds to the number of pegs in that digits row
     * i.e. Row 1 has 1 peg, row 2 has 3 pegs, etc.
     * 9 is prepended to ensure the number always has 5 digits, but is not used
     */
    int board = 91357;  
   while(1) {
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
        read(curPlayer, buffer, 1); //read the move TODO: validate in case of network corruption
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

/* 
 * Parses move request and updates board ineteger
 * board: current state of the board
 * buf: unparsed chars from the user's move
 */
int updateBoard(int board, char* buf) {
    int row = strtol(buf, NULL, 10);
    // number of pegs to remove
    int remove = row % 10;
    row /= 10;
    printf("move: %d %d\n", row, remove);
    //Change remove value to correspond to integer row in board
    int i;
    for (i = 4 - row; i > 0; i--) {
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
