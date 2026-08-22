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

#ifndef ATTACKS_H
#define ATTACKS_H

#ifdef USE_PEXT
#include <immintrin.h>
#endif

#include "types.h"
#include "util.h"

extern BitBoard BETWEEN_SQS[64][64];
extern BitBoard PINNED_MOVES[64][64];

extern BitBoard PAWN_ATTACKS[2][64];
extern BitBoard KNIGHT_ATTACKS[64];
extern BitBoard KING_ATTACKS[64];

// Slider attacks use "fancy" magics: every square owns a slice of a single
// packed table sized to its own number of relevant occupancy bits. The mask,
// magic, shift and table pointer live together so a lookup only touches one
// cache line, and the bishop/rook descriptors for a square share that line.
typedef struct {
  BitBoard mask;
  uint64_t magic;
  const BitBoard* attacks;
  uint64_t shift;
} Magic;

typedef struct {
  Magic bishop;
  Magic rook;
} SquareMagics;

extern SquareMagics MAGICS[64];

void InitBetweenSquares();
void InitPinnedMovementSquares();
void initPawnSpans();
void InitPawnAttacks();
void InitKnightAttacks();
void InitBishopMasks();
void InitBishopMagics();
void InitBishopAttacks();
void InitRookMasks();
void InitRookMagics();
void InitRookAttacks();
void InitKingAttacks();
void InitAttacks();

BitBoard GetGeneratedPawnAttacks(int sq, int color);
BitBoard GetGeneratedKnightAttacks(int sq);
BitBoard GetBishopMask(int sq);
BitBoard GetBishopAttacksOTF(int sq, BitBoard blockers);
BitBoard GetRookMask(int sq);
BitBoard GetRookAttacksOTF(int sq, BitBoard blockers);
BitBoard GetGeneratedKingAttacks(int sq);
BitBoard SetPieceLayoutOccupancy(int idx, int bits, BitBoard attacks);

uint64_t FindMagicNumber(int sq, int n, int bishop);

BitBoard BetweenSquares(int from, int to);
BitBoard PinnedMoves(int p, int k);

BitBoard GetPawnAttacks(int sq, int color);
BitBoard GetKnightAttacks(int sq);
BitBoard GetQueenAttacks(int sq, BitBoard occupancy);
BitBoard GetKingAttacks(int sq);
BitBoard GetPieceAttacks(int sq, BitBoard occupancy, const int type);
BitBoard AttacksToSquare(Board* board, int sq, BitBoard occ);

INLINE BitBoard MagicAttacks(const Magic* m, BitBoard occupancy) {
#ifndef USE_PEXT
  return m->attacks[((occupancy & m->mask) * m->magic) >> m->shift];
#else
  return m->attacks[_pext_u64(occupancy, m->mask)];
#endif
}

INLINE BitBoard GetBishopAttacks(int sq, BitBoard occupancy) {
  return MagicAttacks(&MAGICS[sq].bishop, occupancy);
}

INLINE BitBoard GetRookAttacks(int sq, BitBoard occupancy) {
  return MagicAttacks(&MAGICS[sq].rook, occupancy);
}

#endif
