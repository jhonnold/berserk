// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2021 Jay Honnold

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

#include "types.h"

extern const int BISHOP_RELEVANT_BITS[64];
extern const int ROOK_RELEVANT_BITS[64];

extern BitBoard BETWEEN_SQS[64][64];
extern BitBoard PINNED_MOVES[64][64];
extern BitBoard PAWN_SPANS[2][64];

extern BitBoard PAWN_ATTACKS[2][64];
extern BitBoard KNIGHT_ATTACKS[64];
extern BitBoard BISHOP_ATTACKS[64][512];
extern BitBoard ROOK_ATTACKS[64][4096];
extern BitBoard KING_ATTACKS[64];
extern BitBoard ROOK_MASKS[64];
extern BitBoard BISHOP_MASKS[64];

extern uint64_t ROOK_MAGICS[64];
extern uint64_t BISHOP_MAGICS[64];

void initBetween();
void initPinnedMovement();
void initPawnSpans();
void initPawnAttacks();
void initKnightAttacks();
void initBishopMasks();
void initBishopMagics();
void initBishopAttacks();
void initRookMasks();
void initRookMagics();
void initRookAttacks();
void initKingAttacks();
void initAttacks();

BitBoard getGeneratedPawnAttacks(int sq, int color);
BitBoard getGeneratedKnightAttacks(int sq);
BitBoard getBishopMask(int sq);
BitBoard getBishopAttacksOTF(int sq, BitBoard blockers);
BitBoard getRookMask(int sq);
BitBoard getRookAttacksOTF(int sq, BitBoard blockers);
BitBoard getGeneratedKingAttacks(int sq);
BitBoard setOccupancy(int idx, int bits, BitBoard attacks);

uint64_t findMagicNumber(int sq, int n, int bishop);

BitBoard getInBetween(int from, int to);
BitBoard getPinnedMoves(int p, int k);
BitBoard getPawnSpans(BitBoard pawns, int side);
BitBoard getPawnSpan(int sq, int side);

BitBoard getPawnAttacks(int sq, int color);
BitBoard getKnightAttacks(int sq);
BitBoard getBishopAttacks(int sq, BitBoard occupancy);
BitBoard getRookAttacks(int sq, BitBoard occupancy);
BitBoard getQueenAttacks(int sq, BitBoard occupancy);
BitBoard getKingAttacks(int sq);
BitBoard attacksTo(Board* board, int sq);

#endif
