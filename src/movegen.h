#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "types.h"

#define buildMove(start, end, piece, promo, cap, dub, ep, castle)                                                      \
  (start) | ((end) << 6) | ((piece) << 12) | ((promo) << 16) | ((cap) << 20) | ((dub) << 21) | ((ep) << 22) |          \
      ((castle) << 23)
#define moveStart(m) ((m)&0x3f)
#define moveEnd(m) (((m)&0xfc0) >> 6)
#define movePiece(m) (((m)&0xf000) >> 12)
#define movePromo(m) (((m)&0xf0000) >> 16)
#define moveCapture(m) (((m)&0x100000) >> 20)
#define moveDouble(m) (((m)&0x200000) >> 21)
#define moveEP(m) (((m)&0x400000) >> 22)
#define moveCastle(m) (((m)&0x800000) >> 23)
#define moveSE(m) ((m)&0xfff)

extern const int pawnDirections[];
extern const char* promotionChars;

void addMove(MoveList* moveList, Move move);
void generateMoves(MoveList* moveList, Board* board, int ply);
void generateQuiesceMoves(MoveList* moveList, Board* board, int ply);
void printMoves(MoveList* moveList);
Move parseMove(char* moveStr, Board* board);
char* moveStr(Move move);
void bubbleTopMove(MoveList* moveList, int from);
int see(Board* board, int side, Move move);
void addKiller(Board* board, Move move, int ply);
void addHistory(Board* board, Move move, int depth);
void addBf(Board* board, Move move, int depth);
void addCounter(Board* board, Move move, Move parent);

#endif
