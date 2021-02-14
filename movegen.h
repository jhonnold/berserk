#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "types.h"

#define buildMove(start, end, piece, promo, cap, dub, ep, castle) (start) | (end << 6) | (piece << 12) | ((promo) << 16) | (cap << 20) | (dub << 21) | (ep << 22) | (castle << 23)
#define moveStart(m) (m & 0x3f)
#define moveEnd(m) ((m & 0xfc0) >> 6)
#define movePiece(m) ((m & 0xf000) >> 12)
#define movePromo(m) ((m & 0xf0000) >> 16)
#define moveCapture(m) ((m & 0x100000) >> 20)
#define moveDouble(m) ((m & 0x200000) >> 21)
#define moveEP(m) ((m & 0x400000) >> 22)
#define moveCastle(m) ((m & 0x800000) >> 23)

extern const int pawnDirections[];

void addMove(moves_t *moveList, move_t m);
void generateMoves(moves_t *moveList);
void printMoves(moves_t *moveList);

#endif
