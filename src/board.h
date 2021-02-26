#ifndef BOARD_H
#define BOARD_H

#include "types.h"

extern const int casltingRights[];
extern const int mirror[];

void clear(Board* board);
uint64_t zobrist(Board* board);
void parseFen(char* fen, Board* board);
void printBoard(Board* board);

void setSpecialPieces(Board* board);
void setOccupancies(Board* board);

int inCheck(Board* board);
int isSquareAttacked(int sq, int attacker, BitBoard occupancy, Board* board);
int isLegal(Move move, Board* board);
int isRepetition(Board* board);

int capturedPiece(Move move, Board* board);

void nullMove(Board* board);
void undoNullMove(Board* board);
void makeMove(Move move, Board* board);
void undoMove(Move move, Board* board);

#endif