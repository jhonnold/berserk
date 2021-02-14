#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "attacks.h"
#include "bits.h"
#include "random.h"

const bb_t NOT_A_FILE = 18374403900871474942ULL;
const bb_t NOT_H_FILE = 9187201950435737471ULL;
const bb_t NOT_AB_FILE = 18229723555195321596ULL;
const bb_t NOT_GH_FILE = 4557430888798830399ULL;

const int BISHOP_RELEVANT_BITS[64] = {6, 5, 5, 5, 5, 5, 5, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 7, 7, 7, 7, 5, 5, 5, 5, 7, 9, 9, 7, 5, 5,
                                      5, 5, 7, 9, 9, 7, 5, 5, 5, 5, 7, 7, 7, 7, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 5, 5, 5, 5, 5, 5, 6};

const int ROOK_RELEVANT_BITS[64] = {12, 11, 11, 11, 11, 11, 11, 12, 11, 10, 10, 10, 10, 10, 10, 11, 11, 10, 10, 10, 10, 10, 10, 11, 11, 10, 10, 10, 10, 10, 10, 11,
                                    11, 10, 10, 10, 10, 10, 10, 11, 11, 10, 10, 10, 10, 10, 10, 11, 11, 10, 10, 10, 10, 10, 10, 11, 12, 11, 11, 11, 11, 11, 11, 12};

bb_t PAWN_ATTACKS[2][64];
bb_t KNIGHT_ATTACKS[64];
bb_t BISHOP_ATTACKS[64][512];
bb_t ROOK_ATTACKS[64][4096];
bb_t KING_ATTACKS[64];
bb_t ROOK_MASKS[64];
bb_t BISHOP_MASKS[64];

uint64_t ROOK_MAGICS[64];
uint64_t BISHOP_MAGICS[64];

inline bb_t shift(bb_t bb, int dir) {
  return dir == -8    ? bb >> 8
         : dir == 8   ? bb << 8
         : dir == -16 ? bb >> 16
         : dir == 16  ? bb << 16
         : dir == -1  ? (bb & NOT_A_FILE) >> 1
         : dir == 1   ? (bb & NOT_H_FILE) << 1
         : dir == -7  ? (bb & NOT_H_FILE) >> 7
         : dir == 7   ? (bb & NOT_A_FILE) << 7
         : dir == -9  ? (bb & NOT_A_FILE) >> 9
         : dir == 9   ? (bb & NOT_H_FILE) << 9
                      : 0;
}

bb_t getGeneratedPawnAttacks(int sq, int color) {
  bb_t attacks = 0, board = 0;

  setBit(board, sq);

  if (color) {
    if ((board << 7) & NOT_H_FILE)
      attacks |= (board << 7);
    if ((board << 9) & NOT_A_FILE)
      attacks |= (board << 9);

  } else {
    if ((board >> 7) & NOT_A_FILE)
      attacks |= (board >> 7);
    if ((board >> 9) & NOT_H_FILE)
      attacks |= (board >> 9);
  }

  return attacks;
}

void initPawnAttacks() {
  for (int i = 0; i < 64; i++) {
    PAWN_ATTACKS[0][i] = getGeneratedPawnAttacks(i, 0);
    PAWN_ATTACKS[1][i] = getGeneratedPawnAttacks(i, 1);
  }
}

bb_t getGeneratedKnightAttacks(int sq) {
  bb_t attacks = 0, board = 0;

  setBit(board, sq);

  if ((board >> 17) & NOT_H_FILE)
    attacks |= (board >> 17);
  if ((board >> 15) & NOT_A_FILE)
    attacks |= (board >> 15);
  if ((board >> 10) & NOT_GH_FILE)
    attacks |= (board >> 10);
  if ((board >> 6) & NOT_AB_FILE)
    attacks |= (board >> 6);

  if ((board << 17) & NOT_A_FILE)
    attacks |= (board << 17);
  if ((board << 15) & NOT_H_FILE)
    attacks |= (board << 15);
  if ((board << 10) & NOT_AB_FILE)
    attacks |= (board << 10);
  if ((board << 6) & NOT_GH_FILE)
    attacks |= (board << 6);

  return attacks;
}

void initKnightAttacks() {
  for (int i = 0; i < 64; i++)
    KNIGHT_ATTACKS[i] = getGeneratedKnightAttacks(i);
}

bb_t getGeneratedKingAttacks(int sq) {
  bb_t attacks = 0, board = 0;

  setBit(board, sq);

  attacks |= (board >> 8);
  if ((board >> 7) & NOT_A_FILE)
    attacks |= (board >> 7);
  if ((board >> 9) & NOT_H_FILE)
    attacks |= (board >> 9);
  if ((board >> 1) & NOT_H_FILE)
    attacks |= (board >> 1);

  attacks |= (board << 8);
  if ((board << 7) & NOT_H_FILE)
    attacks |= (board << 7);
  if ((board << 9) & NOT_A_FILE)
    attacks |= (board << 9);
  if ((board << 1) & NOT_A_FILE)
    attacks |= (board << 1);

  return attacks;
}

void initKingAttacks() {
  for (int i = 0; i < 64; i++)
    KING_ATTACKS[i] = getGeneratedKingAttacks(i);
}

bb_t getBishopMask(int sq) {
  bb_t attacks = 0;

  int sr = sq >> 3;
  int sf = sq & 7;

  for (int r = sr + 1, f = sf + 1; r <= 6 && f <= 6; r++, f++)
    attacks |= (1ULL << (r * 8 + f));
  for (int r = sr - 1, f = sf + 1; r >= 1 && f <= 6; r--, f++)
    attacks |= (1ULL << (r * 8 + f));
  for (int r = sr + 1, f = sf - 1; r <= 6 && f >= 1; r++, f--)
    attacks |= (1ULL << (r * 8 + f));
  for (int r = sr - 1, f = sf - 1; r >= 1 && f >= 1; r--, f--)
    attacks |= (1ULL << (r * 8 + f));

  return attacks;
}

void initBishopMasks() {
  for (int i = 0; i < 64; i++) {
    BISHOP_MASKS[i] = getBishopMask(i);
  }
}

bb_t getBishopAttacksOTF(int sq, bb_t blockers) {
  bb_t attacks = 0;

  int sr = sq >> 3;
  int sf = sq & 7;

  for (int r = sr + 1, f = sf + 1; r <= 7 && f <= 7; r++, f++) {
    attacks |= (1ULL << (r * 8 + f));
    if (getBit(blockers, r * 8 + f))
      break;
  }

  for (int r = sr - 1, f = sf + 1; r >= 0 && f <= 7; r--, f++) {
    attacks |= (1ULL << (r * 8 + f));
    if (getBit(blockers, r * 8 + f))
      break;
  }

  for (int r = sr + 1, f = sf - 1; r <= 7 && f >= 0; r++, f--) {
    attacks |= (1ULL << (r * 8 + f));
    if (getBit(blockers, r * 8 + f))
      break;
  }

  for (int r = sr - 1, f = sf - 1; r >= 0 && f >= 0; r--, f--) {
    attacks |= (1ULL << (r * 8 + f));
    if (getBit(blockers, r * 8 + f))
      break;
  }

  return attacks;
}

bb_t getRookMask(int sq) {
  bb_t attacks = 0;

  int sr = sq >> 3;
  int sf = sq & 7;

  for (int r = sr + 1; r <= 6; r++)
    attacks |= (1ULL << (r * 8 + sf));
  for (int r = sr - 1; r >= 1; r--)
    attacks |= (1ULL << (r * 8 + sf));
  for (int f = sf + 1; f <= 6; f++)
    attacks |= (1ULL << (sr * 8 + f));
  for (int f = sf - 1; f >= 1; f--)
    attacks |= (1ULL << (sr * 8 + f));

  return attacks;
}

void initRookMasks() {
  for (int i = 0; i < 64; i++)
    ROOK_MASKS[i] = getRookMask(i);
}

bb_t getRookAttacksOTF(int sq, bb_t blockers) {
  bb_t attacks = 0;

  int sr = sq >> 3;
  int sf = sq & 7;

  for (int r = sr + 1; r <= 7; r++) {
    attacks |= (1ULL << (r * 8 + sf));
    if (getBit(blockers, r * 8 + sf))
      break;
  }

  for (int r = sr - 1; r >= 0; r--) {
    attacks |= (1ULL << (r * 8 + sf));
    if (getBit(blockers, r * 8 + sf))
      break;
  }

  for (int f = sf + 1; f <= 7; f++) {
    attacks |= (1ULL << (sr * 8 + f));
    if (getBit(blockers, sr * 8 + f))
      break;
  }

  for (int f = sf - 1; f >= 0; f--) {
    attacks |= (1ULL << (sr * 8 + f));
    if (getBit(blockers, sr * 8 + f))
      break;
  }

  return attacks;
}

bb_t setOccupancy(int idx, int bits, bb_t attacks) {
  bb_t occupany = 0;

  for (int i = 0; i < bits; i++) {
    int sq = lsb(attacks);
    popLsb(attacks);

    if (idx & (1 << i))
      occupany |= (1ULL << sq);
  }

  return occupany;
}

uint64_t findMagicNumber(int sq, int n, int bishop) {
  int numOccupancies = 1 << n;

  bb_t occupancies[4096];
  bb_t attacks[4096];
  bb_t usedAttacks[4096];

  bb_t mask = bishop ? BISHOP_MASKS[sq] : ROOK_MASKS[sq];

  for (int i = 0; i < numOccupancies; i++) {
    occupancies[i] = setOccupancy(i, n, mask);
    attacks[i] = bishop ? getBishopAttacksOTF(sq, occupancies[i]) : getRookAttacksOTF(sq, occupancies[i]);
  }

  for (int count = 0; count < 10000000; count++) {
    uint64_t magic = randomMagic();

    if (bits((mask * magic) & 0xFF00000000000000) < 6)
      continue;

    memset(usedAttacks, 0UL, sizeof(usedAttacks));

    int failed = 0;
    for (int i = 0; !failed && i < numOccupancies; i++) {
      int idx = (occupancies[i] * magic) >> (64 - n);

      if (!usedAttacks[idx])
        usedAttacks[idx] = attacks[i];
      else if (usedAttacks[idx] != attacks[i])
        failed = 1;
    }

    if (!failed)
      return magic;
  }

  printf("failed to find magic number");
  return 0;
}

void initBishopMagics() {
  for (int i = 0; i < 64; i++)
    BISHOP_MAGICS[i] = findMagicNumber(i, BISHOP_RELEVANT_BITS[i], 1);
}

void initRookMagics() {
  for (int i = 0; i < 64; i++)
    ROOK_MAGICS[i] = findMagicNumber(i, ROOK_RELEVANT_BITS[i], 0);
}

void initBishopAttacks() {
  for (int sq = 0; sq < 64; sq++) {
    bb_t mask = BISHOP_MASKS[sq];
    int bits = BISHOP_RELEVANT_BITS[sq];
    int n = (1 << bits);

    for (int i = 0; i < n; i++) {
      bb_t occupancy = setOccupancy(i, bits, mask);
      int idx = (occupancy * BISHOP_MAGICS[sq]) >> (64 - bits);

      BISHOP_ATTACKS[sq][idx] = getBishopAttacksOTF(sq, occupancy);
    }
  }
}

void initRookAttacks() {
  for (int sq = 0; sq < 64; sq++) {
    bb_t mask = ROOK_MASKS[sq];
    int bits = ROOK_RELEVANT_BITS[sq];
    int n = (1 << bits);

    for (int i = 0; i < n; i++) {
      bb_t occupancy = setOccupancy(i, bits, mask);
      int idx = (occupancy * ROOK_MAGICS[sq]) >> (64 - bits);

      ROOK_ATTACKS[sq][idx] = getRookAttacksOTF(sq, occupancy);
    }
  }
}

void initAttacks() {
  initPawnAttacks();
  initKnightAttacks();
  initKingAttacks();

  initBishopMasks();
  initRookMasks();

  initBishopMagics();
  initRookMagics();

  initBishopAttacks();
  initRookAttacks();
}

bb_t getPawnAttacks(int sq, int color) { return PAWN_ATTACKS[color][sq]; }

bb_t getKnightAttacks(int sq) { return KNIGHT_ATTACKS[sq]; }

bb_t getBishopAttacks(int sq, bb_t occupancy) {
  occupancy &= BISHOP_MASKS[sq];
  occupancy *= BISHOP_MAGICS[sq];
  occupancy >>= 64 - BISHOP_RELEVANT_BITS[sq];

  return BISHOP_ATTACKS[sq][occupancy];
}

bb_t getRookAttacks(int sq, bb_t occupancy) {
  occupancy &= ROOK_MASKS[sq];
  occupancy *= ROOK_MAGICS[sq];
  occupancy >>= 64 - ROOK_RELEVANT_BITS[sq];

  return ROOK_ATTACKS[sq][occupancy];
}

bb_t getQueenAttacks(int sq, bb_t occupancy) { return getBishopAttacks(sq, occupancy) | getRookAttacks(sq, occupancy); }

bb_t getKingAttacks(int sq) { return KING_ATTACKS[sq]; }