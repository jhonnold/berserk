#ifndef BOARD_H
#define BOARD_H

#include "types.h"

extern const int charToPieceIdx[];
extern const char *idxToCord[];
extern const char *pieceChars;

bb_t pieces[12];
bb_t occupancies[3];

int side;
int xside;

int epSquare;
int castling;

int move;

int castlingHistory[512];
int epSquareHistory[512];
int captureHistory[512];

void clear();
void parseFen(char *fen);
void printBoard();
int makeMove(move_t m);
void undoMove(move_t m);
int isSquareAttacked(int sq, int attacker);
int inCheck();

#endif