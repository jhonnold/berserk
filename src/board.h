#ifndef BOARD_H
#define BOARD_H

#include "types.h"

#define NO_PIECE 12

#define file(sq) ((sq)&7)
#define rank(sq) ((sq) >> 3)

extern const BitBoard EMPTY;

extern const int PAWN[];
extern const int KNIGHT[];
extern const int BISHOP[];
extern const int ROOK[];
extern const int QUEEN[];
extern const int KING[];
extern const int PIECE_TYPE[];

extern const int casltingRights[];
extern const int MIRROR[];

extern const uint64_t PIECE_COUNT_IDX[];

uint64_t zobrist(Board* board);

void clear(Board* board);
void parseFen(char* fen, Board* board);
void toFen(char* fen, Board* board);
void printBoard(Board* board);

void setSpecialPieces(Board* board);
void setOccupancies(Board* board);

int isSquareAttacked(int sq, int attacker, BitBoard occupancy, Board* board);
int isLegal(Move move, Board* board);
int isRepetition(Board* board);

int hasNonPawn(Board* board);

void nullMove(Board* board);
void undoNullMove(Board* board);
void makeMove(Move move, Board* board);
void undoMove(Move move, Board* board);

#endif