// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2024 Jay Honnold

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef THREATS_H
#define THREATS_H

#include "../attacks.h"
#include "../bits.h"
#include "../board.h"
#include "../types.h"
#include "../util.h"

// from | to << 8 | attacker << 16 | victim << 20 | add << 31
#define DirtyFrom(d)     ((d) & 63)
#define DirtyTo(d)       (((d) >> 8) & 63)
#define DirtyAttacker(d) (((d) >> 16) & 15)
#define DirtyVictim(d)   (((d) >> 20) & 15)
#define DirtyAdd(d)      ((d) >> 31)

extern int8_t THREAT_WEIGHTS[N_THREATS * N_HIDDEN];

extern int32_t THREAT_PAIR[10][12];
extern uint16_t THREAT_OFFSET[10][64];
extern BitBoard THREAT_EMPTY[10][64];
extern BitBoard RAY_PASS[64][64];

void InitThreats();

int ActiveThreats(Board* board, const int view, int* indices);

INLINE int ThreatIdx(int attacker, int from, int victim, int to, const int view, const int flip) {
  const int block = 5 * ((attacker ^ view) & 1) + (attacker >> 1);
  const int rel   = 6 * ((victim ^ view) & 1) + (victim >> 1);
  const int32_t e = THREAT_PAIR[block][rel];

  from ^= flip;
  to ^= flip;

  if (e < 0 || ((e & 1) && from < to))
    return -1;

  return (e >> 1) + THREAT_OFFSET[block][from] + BitCount(THREAT_EMPTY[block][from] & (Bit(to) - 1));
}

typedef struct {
  uint32_t* dirty;
  int n;
} DirtyThreatList;

INLINE void Push(DirtyThreatList* list, int attacker, int from, int victim, int to, int add) {
  list->dirty[list->n++] = from | (to << 8) | (attacker << 16) | (victim << 20) | ((uint32_t) add << 31);
}

// GetPieceAttacks plus pawns; never called for a king
INLINE BitBoard PieceAttacks(int pc, int sq, BitBoard occ) {
  return PieceType(pc) == PAWN ? GetPawnAttacks(sq, pc & 1) : GetPieceAttacks(sq, occ, PieceType(pc));
}

// MAP as bitboards: the pieces a pt can threaten, and the pieces that can threaten a pt
INLINE BitBoard FeatureVictims(Board* board, int pt) {
  const BitBoard queens = PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK);
  if (pt == PAWN)
    return ~(queens | PieceBB(BISHOP, WHITE) | PieceBB(BISHOP, BLACK));
  if (pt == BISHOP || pt == ROOK)
    return ~queens;
  return ~0ULL;
}

INLINE BitBoard FeatureAttackers(Board* board, int pt) {
  if (pt == BISHOP)
    return ~(PieceBB(PAWN, WHITE) | PieceBB(PAWN, BLACK));
  if (pt == QUEEN)
    return PieceBB(KNIGHT, WHITE) | PieceBB(KNIGHT, BLACK) | PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK);
  return ~0ULL;
}

// The threats a piece on s takes part in, pushed as add. Neither the piece on s
// nor its occupancy bit changes the result, so this runs on either side of the flip.
// Not inlined: it has eight call sites in MakeMoveUpdate. rays is always 1, but
// gcc specialises the function on it and runs ~1% slower without it.
void PieceThreats(Board* board, DirtyThreatList* list, int pc, int s, int add, int rays);

// A capture: occupancy is unchanged, so no lines open or close
INLINE void SwapThreats(Board* board, DirtyThreatList* list, int old, int new, int s) {
  const BitBoard occ   = OccBB(BOTH);
  const BitBoard kings = board->pieces[WHITE_KING] | board->pieces[BLACK_KING];

  const BitBoard in = AttacksToSquare(board, s, occ) & ~kings;

  BitBoard out = PieceAttacks(old, s, occ) & occ & ~kings & FeatureVictims(board, PieceType(old));
  while (out) {
    const int to = PopLSB(&out);
    Push(list, old, s, board->squares[to], to, SUB);
  }

  BitBoard inOld = in & FeatureAttackers(board, PieceType(old));
  while (inOld) {
    const int from = PopLSB(&inOld);
    Push(list, board->squares[from], from, old, s, SUB);
  }

  if (PieceType(new) != KING) {
    BitBoard inNew = in & FeatureAttackers(board, PieceType(new));
    while (inNew) {
      const int from = PopLSB(&inNew);
      Push(list, board->squares[from], from, new, s, ADD);
    }

    out = PieceAttacks(new, s, occ) & occ & ~kings & FeatureVictims(board, PieceType(new));
    while (out) {
      const int to = PopLSB(&out);
      Push(list, new, s, board->squares[to], to, ADD);
    }
  }
}

#endif
