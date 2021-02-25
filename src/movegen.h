#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "types.h"

extern const BitBoard homeRanks[];
extern const BitBoard thirdRanks[];
extern const int pawnDirections[];
extern const char* promotionChars;

void addMove(MoveList* moveList, Move move);
void generateMoves(MoveList* moveList, Board* board, int ply);
void generateQuiesceMoves(MoveList* moveList, Board* board);
void printMoves(MoveList* moveList);
Move parseMove(char* moveStr, Board* board);
char* moveStr(Move move);
void bubbleTopMove(MoveList* moveList, int from);
void addKiller(Board* board, Move move, int ply);
void addCounter(Board* board, Move move, Move parent);

#endif
