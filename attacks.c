#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "attacks.h"
#include "bits.h"
#include "random.h"

const BitBoard NOT_A_FILE = 18374403900871474942ULL;
const BitBoard NOT_H_FILE = 9187201950435737471ULL;
const BitBoard NOT_AB_FILE = 18229723555195321596ULL;
const BitBoard NOT_GH_FILE = 4557430888798830399ULL;

const int BISHOP_RELEVANT_BITS[64] = {6, 5, 5, 5, 5, 5, 5, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 7, 7, 7, 7,
                                      5, 5, 5, 5, 7, 9, 9, 7, 5, 5, 5, 5, 7, 9, 9, 7, 5, 5, 5, 5, 7, 7,
                                      7, 7, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 5, 5, 5, 5, 5, 5, 6};

const int ROOK_RELEVANT_BITS[64] = {12, 11, 11, 11, 11, 11, 11, 12, 11, 10, 10, 10, 10, 10, 10, 11,
                                    11, 10, 10, 10, 10, 10, 10, 11, 11, 10, 10, 10, 10, 10, 10, 11,
                                    11, 10, 10, 10, 10, 10, 10, 11, 11, 10, 10, 10, 10, 10, 10, 11,
                                    11, 10, 10, 10, 10, 10, 10, 11, 12, 11, 11, 11, 11, 11, 11, 12};

BitBoard BETWEEN_SQS[64][64];
BitBoard PINNED_MOVES[64][64];

BitBoard PAWN_ATTACKS[2][64];
BitBoard KNIGHT_ATTACKS[64];
BitBoard BISHOP_ATTACKS[64][512];
BitBoard ROOK_ATTACKS[64][4096];
BitBoard KING_ATTACKS[64];
BitBoard ROOK_MASKS[64];
BitBoard BISHOP_MASKS[64];

uint64_t ROOK_MAGICS[64];
uint64_t BISHOP_MAGICS[64];

inline BitBoard shift(BitBoard bb, int dir) {
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

void initBetween() {
  int i;
  for (int f = 0; f < 64; f++) {
    for (int t = f + 1; t < 64; t++) {
      if ((f >> 3) == (t >> 3)) {
        i = t - 1;
        while (i > f) {
          BETWEEN_SQS[f][t] |= (1ULL << i);
          i--;
        }
      } else if ((f & 7) == (t & 7)) {
        i = t - 8;
        while (i > f) {
          BETWEEN_SQS[f][t] |= (1ULL << i);
          i -= 8;
        }
      } else if ((t - f) % 9 == 0 && ((t & 7) > (f & 7))) {
        i = t - 9;
        while (i > f) {
          BETWEEN_SQS[f][t] |= (1ULL << i);
          i -= 9;
        }
      } else if ((t - f) % 7 == 0 && ((t & 7) < (f & 7))) {
        i = t - 7;
        while (i > f) {
          BETWEEN_SQS[f][t] |= (1ULL << i);
          i -= 7;
        }
      }
    }
  }

  for (int f = 0; f < 64; f++)
    for (int t = 0; t < f; t++)
      BETWEEN_SQS[f][t] = BETWEEN_SQS[t][f];
}

void initPinnedMovement() {
  int dirs[] = {-1, -7, -8, -9, 1, 7, 8, 9};

  for (int pSq = 0; pSq < 64; pSq++) {
    for (int kSq = 0; kSq < 64; kSq++) {
      int dir = 0;
      for (int i = 0; i < 8; i++) {
        if (dir)
          break;

        for (int xray = kSq + dirs[i]; xray >= 0 && xray < 64; xray += dirs[i]) {
          if (dirs[i] == 1 || dirs[i] == 9 || dirs[i] == -7)
            if ((xray & 7) == 0)
              break;

          if (dirs[i] == -1 || dirs[i] == -9 || dirs[i] == 7)
            if ((xray & 7) == 7)
              break;

          if (xray == pSq) {
            dir = dirs[i];
            break;
          }
        }
      }

      if (dir) {
        for (int xray = kSq + dir; xray >= 0 && xray < 64; xray += dir) {
          PINNED_MOVES[pSq][kSq] |= (1ULL << xray);

          if (dir == 1 || dir == 9 || dir == -7)
            if ((xray & 7) == 7)
              break;

          if (dir == -1 || dir == -9 || dir == 7)
            if ((xray & 7) == 0)
              break;
        }
      }
    }
  }
}

inline BitBoard getInBetween(int from, int to) { return BETWEEN_SQS[from][to]; }

inline BitBoard getPinnedMoves(int p, int k) { return PINNED_MOVES[p][k]; }

BitBoard getGeneratedPawnAttacks(int sq, int color) {
  BitBoard attacks = 0, board = 0;

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

BitBoard getGeneratedKnightAttacks(int sq) {
  BitBoard attacks = 0, board = 0;

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

BitBoard getGeneratedKingAttacks(int sq) {
  BitBoard attacks = 0, board = 0;

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

BitBoard getBishopMask(int sq) {
  BitBoard attacks = 0;

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

BitBoard getBishopAttacksOTF(int sq, BitBoard blockers) {
  BitBoard attacks = 0;

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

BitBoard getRookMask(int sq) {
  BitBoard attacks = 0;

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

BitBoard getRookAttacksOTF(int sq, BitBoard blockers) {
  BitBoard attacks = 0;

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

BitBoard setOccupancy(int idx, int bits, BitBoard attacks) {
  BitBoard occupany = 0;

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

  BitBoard occupancies[4096];
  BitBoard attacks[4096];
  BitBoard usedAttacks[4096];

  BitBoard mask = bishop ? BISHOP_MASKS[sq] : ROOK_MASKS[sq];

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
    BitBoard mask = BISHOP_MASKS[sq];
    int bits = BISHOP_RELEVANT_BITS[sq];
    int n = (1 << bits);

    for (int i = 0; i < n; i++) {
      BitBoard occupancy = setOccupancy(i, bits, mask);
      int idx = (occupancy * BISHOP_MAGICS[sq]) >> (64 - bits);

      BISHOP_ATTACKS[sq][idx] = getBishopAttacksOTF(sq, occupancy);
    }
  }
}

void initRookAttacks() {
  for (int sq = 0; sq < 64; sq++) {
    BitBoard mask = ROOK_MASKS[sq];
    int bits = ROOK_RELEVANT_BITS[sq];
    int n = (1 << bits);

    for (int i = 0; i < n; i++) {
      BitBoard occupancy = setOccupancy(i, bits, mask);
      int idx = (occupancy * ROOK_MAGICS[sq]) >> (64 - bits);

      ROOK_ATTACKS[sq][idx] = getRookAttacksOTF(sq, occupancy);
    }
  }
}

void initAttacks() {
  initBetween();
  initPinnedMovement();

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

inline BitBoard getPawnAttacks(int sq, int color) { return PAWN_ATTACKS[color][sq]; }

inline BitBoard getKnightAttacks(int sq) { return KNIGHT_ATTACKS[sq]; }

inline BitBoard getBishopAttacks(int sq, BitBoard occupancy) {
  occupancy &= BISHOP_MASKS[sq];
  occupancy *= BISHOP_MAGICS[sq];
  occupancy >>= 64 - BISHOP_RELEVANT_BITS[sq];

  return BISHOP_ATTACKS[sq][occupancy];
}

inline BitBoard getRookAttacks(int sq, BitBoard occupancy) {
  occupancy &= ROOK_MASKS[sq];
  occupancy *= ROOK_MAGICS[sq];
  occupancy >>= 64 - ROOK_RELEVANT_BITS[sq];

  return ROOK_ATTACKS[sq][occupancy];
}

inline BitBoard getQueenAttacks(int sq, BitBoard occupancy) {
  return getBishopAttacks(sq, occupancy) | getRookAttacks(sq, occupancy);
}

inline BitBoard getKingAttacks(int sq) { return KING_ATTACKS[sq]; }