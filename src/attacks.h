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

// Hyperbola quintessence computes slider attacks arithmetically instead of
// reading a magic table, so it needs no lookup memory beyond a per square
// descriptor. It only pays off with the 256 bit shuffles, so it is limited to
// AVX2, and PEXT builds keep their existing table lookup.
#if defined(__AVX2__)
#define USE_DUAL_HQ
#endif

#ifdef USE_DUAL_HQ
#include <immintrin.h>
#endif

#include "board.h"
#include "types.h"
#include "util.h"

extern BitBoard BETWEEN_SQS[64][64];
extern BitBoard PINNED_MOVES[64][64];

extern BitBoard PAWN_ATTACKS[2][64];
extern BitBoard KNIGHT_ATTACKS[64];
extern BitBoard KING_ATTACKS[64];

// Slider attacks on an empty board. Deriving these through the normal slider
// path costs a full hyperbola computation for an occupancy that is known to be
// zero, so the two rays are just tabulated.
extern BitBoard BISHOP_RAYS[64];
extern BitBoard ROOK_RAYS[64];

#ifdef USE_DUAL_HQ

// One descriptor per square, sized to a single cache line. The first four
// quadwords are the empty board rays used as hyperbola quintessence masks and
// are loaded as one 256 bit vector, so both sliders are resolved by a single
// pass: lane 0 yields the rook's file attacks and lanes 1|3 the bishop's two
// diagonals. Rank attacks cannot use the byte reversal the other three rays
// rely on (a rank shares one byte), so they come from a 64 entry table indexed
// by the rank's inner occupancy. Total cost is 4KB of descriptors and 512
// bytes of rank attacks, against 841KB of magic tables.
typedef struct {
  BitBoard maskFile, maskDiag, maskNone, maskAntiDiag;
  BitBoard r, rr;
  const uint8_t* rankAttacks;
  uint64_t shift;
} DualMagic;

extern DualMagic DUAL_MAGICS[64];

void InitDualMagics();

#else

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

#endif

void InitBetweenSquares();
void InitPinnedMovementSquares();
void initPawnSpans();
void InitPawnAttacks();
void InitKnightAttacks();
void InitKingAttacks();
void InitSliderRays();
#ifndef USE_DUAL_HQ
void InitBishopMasks();
void InitBishopMagics();
void InitBishopAttacks();
void InitRookMasks();
void InitRookMagics();
void InitRookAttacks();
#endif
void InitAttacks();

BitBoard GetGeneratedPawnAttacks(int sq, int color);
BitBoard GetGeneratedKnightAttacks(int sq);
BitBoard GetBishopAttacksOTF(int sq, BitBoard blockers);
BitBoard GetRookAttacksOTF(int sq, BitBoard blockers);
BitBoard GetGeneratedKingAttacks(int sq);
#ifndef USE_DUAL_HQ
BitBoard GetBishopMask(int sq);
BitBoard GetRookMask(int sq);
BitBoard SetPieceLayoutOccupancy(int idx, int bits, BitBoard attacks);

uint64_t FindMagicNumber(int sq, int n, int bishop);
#endif


#ifdef USE_DUAL_HQ

// Returns the rook's file attacks in the low quadword and the bishop's full
// attacks in the high one. Callers that need both sliders on the same square
// share this, the compiler folds the second call away.
INLINE __m128i DualHyperbola(const DualMagic* m, BitBoard occupancy) {
  // Reverses the 16 bytes of each 128 bit half, which both swaps the two
  // quadwords and reverses the bytes within them.
  const __m256i rev8 = _mm256_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, //
                                       0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

  const __m256i mask = _mm256_load_si256((const __m256i*) m);
  const __m256i occ  = _mm256_and_si256(mask, _mm256_set1_epi64x((long long) occupancy));
  const __m256i fwd  = _mm256_sub_epi64(occ, _mm256_set1_epi64x((long long) m->r));
  const __m256i rev  = _mm256_shuffle_epi8(
    _mm256_sub_epi64(_mm256_shuffle_epi8(occ, rev8), _mm256_set1_epi64x((long long) m->rr)), rev8);
  const __m256i attacks = _mm256_and_si256(_mm256_xor_si256(fwd, rev), mask);

  // Low half holds [file, diagonal], high half [0, antidiagonal].
  return _mm_or_si128(_mm256_extracti128_si256(attacks, 1), _mm256_castsi256_si128(attacks));
}

INLINE BitBoard GetBishopAttacks(int sq, BitBoard occupancy) {
  return (BitBoard) _mm_extract_epi64(DualHyperbola(&DUAL_MAGICS[sq], occupancy), 1);
}

INLINE BitBoard GetRookAttacks(int sq, BitBoard occupancy) {
  const DualMagic* m = &DUAL_MAGICS[sq];
  return (BitBoard) _mm_cvtsi128_si64(DualHyperbola(m, occupancy)) |
         ((BitBoard) m->rankAttacks[(occupancy >> (m->shift + 1)) & 0x3f] << m->shift);
}

#else

INLINE BitBoard MagicAttacks(const Magic* m, BitBoard occupancy) {
  return m->attacks[((occupancy & m->mask) * m->magic) >> m->shift];
}

INLINE BitBoard GetBishopAttacks(int sq, BitBoard occupancy) {
  return MagicAttacks(&MAGICS[sq].bishop, occupancy);
}

INLINE BitBoard GetRookAttacks(int sq, BitBoard occupancy) {
  return MagicAttacks(&MAGICS[sq].rook, occupancy);
}

#endif


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
