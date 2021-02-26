#ifndef MOVE_H
#define MOVE_H

#include "types.h"

extern const int CHAR_TO_PIECE[];
extern const char* PIECE_TO_CHAR;
extern const char* PROMOTION_TO_CHAR;
extern const char* SQ_TO_COORD[];

#define buildMove(start, end, piece, promo, cap, dub, ep, castle)                                                      \
  (start) | ((end) << 6) | ((piece) << 12) | ((promo) << 16) | ((cap) << 20) | ((dub) << 21) | ((ep) << 22) |          \
      ((castle) << 23)
#define moveStart(move) ((move)&0x3f)
#define moveEnd(move) (((move)&0xfc0) >> 6)
#define movePiece(move) (((move)&0xf000) >> 12)
#define movePromo(move) (((move)&0xf0000) >> 16)
#define moveCapture(move) (((move)&0x100000) >> 20)
#define moveDouble(move) (((move)&0x200000) >> 21)
#define moveEP(move) (((move)&0x400000) >> 22)
#define moveCastle(move) (((move)&0x800000) >> 23)
#define moveSE(move) ((move)&0xfff)

Move parseMove(char* moveStr, Board* board);
char* moveStr(Move move);

#endif