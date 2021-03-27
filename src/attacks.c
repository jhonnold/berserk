#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "movegen.h"
#include "random.h"

// clang-format off
const int BISHOP_RELEVANT_BITS[64] = {
  6, 5, 5, 5, 5, 5, 5, 6, 
  5, 5, 5, 5, 5, 5, 5, 5, 
  5, 5, 7, 7, 7, 7, 5, 5, 
  5, 5, 7, 9, 9, 7, 5, 5, 
  5, 5, 7, 9, 9, 7, 5, 5, 
  5, 5, 7, 7, 7, 7, 5, 5, 
  5, 5, 5, 5, 5, 5, 5, 5, 
  6, 5, 5, 5, 5, 5, 5, 6
};

const int ROOK_RELEVANT_BITS[64] = {
  12, 11, 11, 11, 11, 11, 11, 12, 
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11, 
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11, 
  11, 10, 10, 10, 10, 10, 10, 11,
  11, 10, 10, 10, 10, 10, 10, 11, 
  12, 11, 11, 11, 11, 11, 11, 12
};
// clang-format on

BitBoard BETWEEN_SQS[64][64];
BitBoard PINNED_MOVES[64][64];
BitBoard PAWN_SPANS[2][64];

BitBoard PAWN_ATTACKS[2][64];
BitBoard KNIGHT_ATTACKS[64];
BitBoard BISHOP_ATTACKS[64][512];
BitBoard ROOK_ATTACKS[64][4096];
BitBoard KING_ATTACKS[64];
BitBoard ROOK_MASKS[64];
BitBoard BISHOP_MASKS[64];

uint64_t ROOK_MAGICS[64];
uint64_t BISHOP_MAGICS[64];

void initBetween() {
  int i;
  for (int f = 0; f < 64; f++) {
    for (int t = f + 1; t < 64; t++) {
      if (rank(f) == rank(t)) {
        i = t + W;
        while (i > f) {
          BETWEEN_SQS[f][t] |= (1ULL << i);
          i += W;
        }
      } else if (file(f) == file(t)) {
        i = t + N;
        while (i > f) {
          BETWEEN_SQS[f][t] |= (1ULL << i);
          i += N;
        }
      } else if ((t - f) % 9 == 0 && (file(t) > file(f))) {
        i = t + NW;
        while (i > f) {
          BETWEEN_SQS[f][t] |= (1ULL << i);
          i += NW;
        }
      } else if ((t - f) % 7 == 0 && (file(t) < file(f))) {
        i = t + NE;
        while (i > f) {
          BETWEEN_SQS[f][t] |= (1ULL << i);
          i += NE;
        }
      }
    }
  }

  for (int f = 0; f < 64; f++)
    for (int t = 0; t < f; t++)
      BETWEEN_SQS[f][t] = BETWEEN_SQS[t][f];
}

void initPinnedMovement() {
  int dirs[] = {W, NE, N, NW, E, SW, S, SE};

  for (int pSq = 0; pSq < 64; pSq++) {
    for (int kSq = 0; kSq < 64; kSq++) {
      int dir = 0;
      for (int i = 0; i < 8; i++) {
        if (dir)
          break;

        for (int xray = kSq + dirs[i]; xray >= 0 && xray < 64; xray += dirs[i]) {
          if (dirs[i] == E || dirs[i] == SE || dirs[i] == NE)
            if (file(xray) == 0)
              break;

          if (dirs[i] == W || dirs[i] == NW || dirs[i] == SW)
            if (file(xray) == 7)
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

          if (dir == E || dir == SE || dir == NE)
            if (file(xray) == 7)
              break;

          if (dir == W || dir == SW || dir == NW)
            if (file(xray) == 0)
              break;
        }
      }
    }
  }
}

void initPawnSpans() {
  // WHITE
  for (int sq = 0; sq < 64; sq++) {
    BitBoard span = 0;
    for (int i = sq + PAWN_DIRECTIONS[WHITE]; i >= 0; i += PAWN_DIRECTIONS[WHITE]) {
      setBit(span, i);
    }

    span |= PAWN_SPANS[WHITE][sq] = span | shift(span, E) | shift(span, W);
  }

  // BLACK
  for (int sq = 0; sq < 64; sq++) {
    BitBoard span = 0;
    for (int i = sq + PAWN_DIRECTIONS[BLACK]; i < 64; i += PAWN_DIRECTIONS[BLACK]) {
      setBit(span, i);
    }

    span |= PAWN_SPANS[BLACK][sq] = span | shift(span, E) | shift(span, W);
  }
}

inline BitBoard getInBetween(int from, int to) { return BETWEEN_SQS[from][to]; }

inline BitBoard getPinnedMoves(int p, int k) { return PINNED_MOVES[p][k]; }

inline BitBoard getPawnSpan(int sq, int side) { return PAWN_SPANS[side][sq]; }

inline BitBoard getPawnSpans(BitBoard pawns, int side) {
  BitBoard span = 0;
  for (; pawns; popLsb(pawns))
    span |= PAWN_SPANS[side][lsb(pawns)];

  return span;
}

BitBoard getGeneratedPawnAttacks(int sq, int color) {
  BitBoard attacks = 0, board = 0;

  setBit(board, sq);

  if (color == WHITE) {
    attacks |= shift(board, NW);
    attacks |= shift(board, NE);
  } else {
    attacks |= shift(board, SE);
    attacks |= shift(board, SW);
  }

  return attacks;
}

void initPawnAttacks() {
  for (int i = 0; i < 64; i++) {
    PAWN_ATTACKS[WHITE][i] = getGeneratedPawnAttacks(i, WHITE);
    PAWN_ATTACKS[BLACK][i] = getGeneratedPawnAttacks(i, BLACK);
  }
}

BitBoard getGeneratedKnightAttacks(int sq) {
  BitBoard attacks = 0, board = 0;

  setBit(board, sq);

  if ((board >> 17) & ~H_FILE)
    attacks |= (board >> 17);
  if ((board >> 15) & ~A_FILE)
    attacks |= (board >> 15);
  if ((board >> 10) & ~(G_FILE | H_FILE))
    attacks |= (board >> 10);
  if ((board >> 6) & ~(A_FILE | B_FILE))
    attacks |= (board >> 6);

  if ((board << 17) & ~A_FILE)
    attacks |= (board << 17);
  if ((board << 15) & ~H_FILE)
    attacks |= (board << 15);
  if ((board << 10) & ~(A_FILE | B_FILE))
    attacks |= (board << 10);
  if ((board << 6) & ~(G_FILE | H_FILE))
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

  attacks |= shift(board, N);
  attacks |= shift(board, NE);
  attacks |= shift(board, E);
  attacks |= shift(board, SE);
  attacks |= shift(board, S);
  attacks |= shift(board, SW);
  attacks |= shift(board, W);
  attacks |= shift(board, NW);

  return attacks;
}

void initKingAttacks() {
  for (int i = 0; i < 64; i++)
    KING_ATTACKS[i] = getGeneratedKingAttacks(i);
}

BitBoard getBishopMask(int sq) {
  BitBoard attacks = 0;

  int sr = rank(sq);
  int sf = file(sq);

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

  int sr = rank(sq);
  int sf = file(sq);

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

  int sr = rank(sq);
  int sf = file(sq);

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

  int sr = rank(sq);
  int sf = file(sq);

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

inline BitBoard attacksTo(Board* board, int sq) {
  BitBoard attacks = (getPawnAttacks(sq, WHITE) & board->pieces[PAWN[BLACK]]) |
                     (getPawnAttacks(sq, BLACK) & board->pieces[PAWN[WHITE]]) |
                     (getKnightAttacks(sq) & (board->pieces[KNIGHT[WHITE]] | board->pieces[KNIGHT[BLACK]])) |
                     (getKingAttacks(sq) & (board->pieces[KING[WHITE]] | board->pieces[KING[BLACK]]));

  attacks |=
      getBishopAttacks(sq, board->occupancies[BOTH]) & (board->pieces[BISHOP[WHITE]] | board->pieces[BISHOP[BLACK]] |
                                                        board->pieces[QUEEN[WHITE]] | board->pieces[QUEEN[BLACK]]);
  attacks |= getRookAttacks(sq, board->occupancies[BOTH]) & (board->pieces[ROOK[WHITE]] | board->pieces[ROOK[BLACK]] |
                                                             board->pieces[QUEEN[WHITE]] | board->pieces[QUEEN[BLACK]]);

  return attacks;
}