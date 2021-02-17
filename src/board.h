#ifndef BOARD_H
#define BOARD_H

#include "types.h"

extern const int charToPieceIdx[];
extern const char* idxToCord[];
extern const char* pieceChars;
extern const int mirror[];

void clear(Board* board);
void parseFen(char* fen, Board* board);
void printBoard(Board* board);
void makeMove(Move move, Board* board);
void undoMove(Move move, Board* board);
int isSquareAttacked(int sq, int attacker, BitBoard occupancy, Board* board);
int inCheck(Board* board);
int isLegal(Move move, Board* board);
int isRepetition(Board* board);
void nullMove(Board* board);
void undoNullMove(Board* board);

#endif