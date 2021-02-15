#ifndef BOARD_H
#define BOARD_H

#include "types.h"

extern const int charToPieceIdx[];
extern const char *idxToCord[];
extern const char *pieceChars;

bb_t pieces[12];
bb_t occupancies[3];
bb_t checkers;
bb_t pinned;

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
void makeMove(move_t m);
void undoMove(move_t m);
int isSquareAttacked(int sq, int attacker, bb_t occupancy);
int inCheck();
int isLegal(move_t m);
void setOccupancies();
void setSpecialPieces();

#endif