// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2023 Jay Honnold

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
extern BitBoard BISHOP_ATTACKS[64][512];
extern BitBoard ROOK_ATTACKS[64][4096];
extern BitBoard KING_ATTACKS[64];
extern BitBoard ROOK_MASKS[64];
extern BitBoard BISHOP_MASKS[64];

extern uint64_t ROOK_MAGICS[64];
extern uint64_t BISHOP_MAGICS[64];

void InitAttacks();

INLINE BitBoard BetweenSquares(int from, int to) {
  return BETWEEN_SQS[from][to];
}

INLINE BitBoard PinnedMoves(int p, int k) {
  return PINNED_MOVES[p][k];
}

INLINE BitBoard GetPawnAttacks(int sq, int color) {
  return PAWN_ATTACKS[color][sq];
}

INLINE BitBoard GetKnightAttacks(int sq) {
  return KNIGHT_ATTACKS[sq];
}

INLINE BitBoard GetBishopAttacks(int sq, BitBoard occupancy) {
#ifndef USE_PEXT
  occupancy &= BISHOP_MASKS[sq];
  occupancy *= BISHOP_MAGICS[sq];
  occupancy >>= 64 - BISHOP_RELEVANT_BITS[sq];

  return BISHOP_ATTACKS[sq][occupancy];
#else
  return BISHOP_ATTACKS[sq][_pext_u64(occupancy, BISHOP_MASKS[sq])];
#endif
}

INLINE BitBoard GetRookAttacks(int sq, BitBoard occupancy) {
#ifndef USE_PEXT
  occupancy &= ROOK_MASKS[sq];
  occupancy *= ROOK_MAGICS[sq];
  occupancy >>= 64 - ROOK_RELEVANT_BITS[sq];

  return ROOK_ATTACKS[sq][occupancy];
#else
  return ROOK_ATTACKS[sq][_pext_u64(occupancy, ROOK_MASKS[sq])];
#endif
}

INLINE BitBoard GetQueenAttacks(int sq, BitBoard occupancy) {
  return GetBishopAttacks(sq, occupancy) | GetRookAttacks(sq, occupancy);
}

INLINE BitBoard GetKingAttacks(int sq) {
  return KING_ATTACKS[sq];
}

INLINE BitBoard GetPieceAttacks(int sq, BitBoard occupancy, const int type) {
  switch (type) {
    case KNIGHT: return GetKnightAttacks(sq);
    case BISHOP: return GetBishopAttacks(sq, occupancy);
    case ROOK: return GetRookAttacks(sq, occupancy);
    case QUEEN: return GetQueenAttacks(sq, occupancy);
    case KING: return GetKingAttacks(sq);
  }

  return 0;
}

// get a bitboard of ALL pieces attacking a given square
INLINE BitBoard AttacksToSquare(Board* board, int sq, BitBoard occ) {
  return (GetPawnAttacks(sq, WHITE) & PieceBB(PAWN, BLACK)) |                            // White and Black Pawn atx
         (GetPawnAttacks(sq, BLACK) & PieceBB(PAWN, WHITE)) |                            //
         (GetKnightAttacks(sq) & (PieceBB(KNIGHT, WHITE) | PieceBB(KNIGHT, BLACK))) |    // Knights
         (GetKingAttacks(sq) & (PieceBB(KING, WHITE) | PieceBB(KING, BLACK))) |          // Kings
         (GetBishopAttacks(sq, occ) & (PieceBB(BISHOP, WHITE) | PieceBB(BISHOP, BLACK) | // Bishop + Queen
                                       PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK))) | //
         (GetRookAttacks(sq, occ) & (PieceBB(ROOK, WHITE) | PieceBB(ROOK, BLACK) |       // Rook + Queen
                                     PieceBB(QUEEN, WHITE) | PieceBB(QUEEN, BLACK)));
}

#endif
