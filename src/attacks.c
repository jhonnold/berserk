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

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "movegen.h"
#include "random.h"

// This file was built using all the logic found in the BBC video guide on
// youtube I highly recommend it to understand how magic bitboards
// work/generated
// https://www.youtube.com/channel/UCB9-prLkPwgvlKKqDgXhsMQ/videos
// OTF is abbr for On The Fly

#ifndef USE_DUAL_HQ

const int BISHOP_RELEVANT_BITS[64] = {6, 5, 5, 5, 5, 5, 5, 6, //
                                      5, 5, 5, 5, 5, 5, 5, 5, //
                                      5, 5, 7, 7, 7, 7, 5, 5, //
                                      5, 5, 7, 9, 9, 7, 5, 5, //
                                      5, 5, 7, 9, 9, 7, 5, 5, //
                                      5, 5, 7, 7, 7, 7, 5, 5, //
                                      5, 5, 5, 5, 5, 5, 5, 5, //
                                      6, 5, 5, 5, 5, 5, 5, 6};

const int ROOK_RELEVANT_BITS[64] = {12, 11, 11, 11, 11, 11, 11, 12, //
                                    11, 10, 10, 10, 10, 10, 10, 11, //
                                    11, 10, 10, 10, 10, 10, 10, 11, //
                                    11, 10, 10, 10, 10, 10, 10, 11, //
                                    11, 10, 10, 10, 10, 10, 10, 11, //
                                    11, 10, 10, 10, 10, 10, 10, 11, //
                                    11, 10, 10, 10, 10, 10, 10, 11, //
                                    12, 11, 11, 11, 11, 11, 11, 12};

#endif

BitBoard BETWEEN_SQS[64][64];
BitBoard PINNED_MOVES[64][64];

BitBoard PAWN_ATTACKS[2][64];
BitBoard KNIGHT_ATTACKS[64];
BitBoard KING_ATTACKS[64];

BitBoard BISHOP_RAYS[64];
BitBoard ROOK_RAYS[64];

#ifndef USE_DUAL_HQ

// Sum of (1 << relevant bits) over all 64 squares for each slider.
#define BISHOP_TABLE_SIZE 5248
#define ROOK_TABLE_SIZE   102400

SquareMagics MAGICS[64] ALIGN;

static BitBoard BISHOP_TABLE[BISHOP_TABLE_SIZE] ALIGN;
static BitBoard ROOK_TABLE[ROOK_TABLE_SIZE] ALIGN;

static const uint64_t ROOK_MAGIC_NUMBERS[64] = {
    0x80800015c0082080ULL, 0x00c0100140002000ULL, 0x0100104009042000ULL, 0x0480080080100004ULL, 0x1080040008008002ULL, 0x1200080410020001ULL, 0x030004ca00040500ULL, 0x408000a480004900ULL,
    0x0422800024904000ULL, 0x6000400050002001ULL, 0x1221002005021240ULL, 0x0000808008001000ULL, 0x2050808004000800ULL, 0x0042000200080410ULL, 0x1004000241084410ULL, 0x080200040484690aULL,
    0x81c0808000284004ULL, 0x09c0018020008040ULL, 0x4000420020801200ULL, 0x4008008010000882ULL, 0x8038008004008008ULL, 0x0802808002000400ULL, 0x0c10440021020890ULL, 0x0000020000840041ULL,
    0x0080005040002001ULL, 0x0000400480200080ULL, 0x0000104100200101ULL, 0x0010001080800800ULL, 0x0000040080800800ULL, 0x2048020080040080ULL, 0x0880120400810850ULL, 0x028809020004884cULL,
    0x2440102040800086ULL, 0x1020100020404000ULL, 0x0861200184801000ULL, 0x0200801000800800ULL, 0x0010080080800400ULL, 0x2202001002000409ULL, 0x04401022040008a1ULL, 0x0802049106000054ULL,
    0x0040804000228000ULL, 0x0050004020014010ULL, 0x2020200010008080ULL, 0x0010008100080800ULL, 0x0028000400088080ULL, 0x4112010488020010ULL, 0x0462000408020001ULL, 0x10400ca841020004ULL,
    0x8006320146810200ULL, 0x0000812000400280ULL, 0x0010144020090100ULL, 0x008a001008452200ULL, 0x0018004004020040ULL, 0x0020020004008080ULL, 0x0006011002080400ULL, 0x0000024700b40200ULL,
    0x0000410080002011ULL, 0x040811042182c001ULL, 0x58201041000a2001ULL, 0x0c00041001210009ULL, 0x000200846010182aULL, 0x0001000208040013ULL, 0x9005000082002441ULL, 0x0484802102c40186ULL,
};

static const uint64_t BISHOP_MAGIC_NUMBERS[64] = {
    0x0020828081010200ULL, 0x4020410421004045ULL, 0x4084080081030428ULL, 0x2002208200400040ULL, 0x40240504102d0220ULL, 0x000a081424100200ULL, 0x0004108410080200ULL, 0x4040808050108400ULL,
    0x2004420822041042ULL, 0x0006101000890054ULL, 0x06085010c0810800ULL, 0x08000444008a080cULL, 0x000a0d1041001000ULL, 0x1040008220600200ULL, 0x0010110110100400ULL, 0x00000830880c1040ULL,
    0x8840400510041108ULL, 0x0502000818510400ULL, 0x02411008080b0010ULL, 0x800406084400080eULL, 0x4801004590400190ULL, 0x8101000080603200ULL, 0x0301110044100400ULL, 0x004020208a080200ULL,
    0x010844180aa01800ULL, 0x0904204004588880ULL, 0x1218510908020400ULL, 0x9008080040202120ULL, 0x0120840202802000ULL, 0x5118024004806020ULL, 0x0942088684040120ULL, 0x0009010190440891ULL,
    0x3041101021882010ULL, 0x1000822040080801ULL, 0x0410280800010a00ULL, 0xc020400808038200ULL, 0x0204200200402080ULL, 0x8090004200134100ULL, 0x8110010304204460ULL, 0x4021086200018a00ULL,
    0xc10808a208a01000ULL, 0x0024308818048410ULL, 0x4002010448004101ULL, 0x0402012011008802ULL, 0x0000102012000041ULL, 0x00a1014101004200ULL, 0x0002820424008108ULL, 0xa210010069010880ULL,
    0x0800421011082208ULL, 0x8000804842102000ULL, 0x0400050088040015ULL, 0x0001020084043004ULL, 0x02250c4010410040ULL, 0x200c910210010000ULL, 0x0a12029004108000ULL, 0x8028c84284014009ULL,
    0x00053c0200a2e000ULL, 0x1060102401080822ULL, 0x800404420082210dULL, 0x0100708002050412ULL, 0x1100404240105100ULL, 0x08202120081042c0ULL, 0x0600204801082480ULL, 0x0a02a00202021220ULL,
};

#endif

void InitBetweenSquares() {
  int i;
  for (int f = 0; f < 64; f++) {
    for (int t = f + 1; t < 64; t++) {
      if (Rank(f) == Rank(t)) {
        i = t + W;
        while (i > f) {
          BETWEEN_SQS[f][t] |= (1ULL << i);
          i += W;
        }
      } else if (File(f) == File(t)) {
        i = t + N;
        while (i > f) {
          BETWEEN_SQS[f][t] |= (1ULL << i);
          i += N;
        }
      } else if ((t - f) % 9 == 0 && (File(t) > File(f))) {
        i = t + NW;
        while (i > f) {
          BETWEEN_SQS[f][t] |= (1ULL << i);
          i += NW;
        }
      } else if ((t - f) % 7 == 0 && (File(t) < File(f))) {
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

void InitPinnedMovementSquares() {
  int dirs[] = {W, NE, N, NW, E, SW, S, SE};

  for (int pSq = 0; pSq < 64; pSq++) {
    for (int kSq = 0; kSq < 64; kSq++) {
      int dir = 0;
      for (int i = 0; i < 8; i++) {
        if (dir)
          break;

        for (int xray = kSq + dirs[i]; xray >= 0 && xray < 64; xray += dirs[i]) {
          if (dirs[i] == E || dirs[i] == SE || dirs[i] == NE)
            if (File(xray) == 0)
              break;

          if (dirs[i] == W || dirs[i] == NW || dirs[i] == SW)
            if (File(xray) == 7)
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
            if (File(xray) == 7)
              break;

          if (dir == W || dir == SW || dir == NW)
            if (File(xray) == 0)
              break;
        }
      }
    }
  }
}

BitBoard GetGeneratedPawnAttacks(int sq, int color) {
  BitBoard attacks = 0, board = 0;

  SetBit(board, sq);

  if (color == WHITE) {
    attacks |= ShiftNW(board);
    attacks |= ShiftNE(board);
  } else {
    attacks |= ShiftSE(board);
    attacks |= ShiftSW(board);
  }

  return attacks;
}

void InitPawnAttacks() {
  for (int i = 0; i < 64; i++) {
    PAWN_ATTACKS[WHITE][i] = GetGeneratedPawnAttacks(i, WHITE);
    PAWN_ATTACKS[BLACK][i] = GetGeneratedPawnAttacks(i, BLACK);
  }
}

BitBoard GetGeneratedKnightAttacks(int sq) {
  BitBoard attacks = 0, board = 0;

  SetBit(board, sq);

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

void InitKnightAttacks() {
  for (int i = 0; i < 64; i++)
    KNIGHT_ATTACKS[i] = GetGeneratedKnightAttacks(i);
}

BitBoard GetGeneratedKingAttacks(int sq) {
  BitBoard attacks = 0, board = 0;

  SetBit(board, sq);

  attacks |= ShiftN(board);
  attacks |= ShiftNE(board);
  attacks |= ShiftE(board);
  attacks |= ShiftSE(board);
  attacks |= ShiftS(board);
  attacks |= ShiftSW(board);
  attacks |= ShiftW(board);
  attacks |= ShiftNW(board);

  return attacks;
}

void InitKingAttacks() {
  for (int i = 0; i < 64; i++)
    KING_ATTACKS[i] = GetGeneratedKingAttacks(i);
}

#ifndef USE_DUAL_HQ

BitBoard GetBishopMask(int sq) {
  BitBoard attacks = 0;

  int sr = Rank(sq);
  int sf = File(sq);

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

void InitBishopMasks() {
  for (int i = 0; i < 64; i++)
    MAGICS[i].bishop.mask = GetBishopMask(i);
}

#endif

BitBoard GetBishopAttacksOTF(int sq, BitBoard blockers) {
  BitBoard attacks = 0;

  int sr = Rank(sq);
  int sf = File(sq);

  for (int r = sr + 1, f = sf + 1; r <= 7 && f <= 7; r++, f++) {
    attacks |= (1ULL << (r * 8 + f));
    if (GetBit(blockers, r * 8 + f))
      break;
  }

  for (int r = sr - 1, f = sf + 1; r >= 0 && f <= 7; r--, f++) {
    attacks |= (1ULL << (r * 8 + f));
    if (GetBit(blockers, r * 8 + f))
      break;
  }

  for (int r = sr + 1, f = sf - 1; r <= 7 && f >= 0; r++, f--) {
    attacks |= (1ULL << (r * 8 + f));
    if (GetBit(blockers, r * 8 + f))
      break;
  }

  for (int r = sr - 1, f = sf - 1; r >= 0 && f >= 0; r--, f--) {
    attacks |= (1ULL << (r * 8 + f));
    if (GetBit(blockers, r * 8 + f))
      break;
  }

  return attacks;
}

#ifndef USE_DUAL_HQ

BitBoard GetRookMask(int sq) {
  BitBoard attacks = 0;

  int sr = Rank(sq);
  int sf = File(sq);

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

void InitRookMasks() {
  for (int i = 0; i < 64; i++)
    MAGICS[i].rook.mask = GetRookMask(i);
}

#endif

BitBoard GetRookAttacksOTF(int sq, BitBoard blockers) {
  BitBoard attacks = 0;

  int sr = Rank(sq);
  int sf = File(sq);

  for (int r = sr + 1; r <= 7; r++) {
    attacks |= (1ULL << (r * 8 + sf));
    if (GetBit(blockers, r * 8 + sf))
      break;
  }

  for (int r = sr - 1; r >= 0; r--) {
    attacks |= (1ULL << (r * 8 + sf));
    if (GetBit(blockers, r * 8 + sf))
      break;
  }

  for (int f = sf + 1; f <= 7; f++) {
    attacks |= (1ULL << (sr * 8 + f));
    if (GetBit(blockers, sr * 8 + f))
      break;
  }

  for (int f = sf - 1; f >= 0; f--) {
    attacks |= (1ULL << (sr * 8 + f));
    if (GetBit(blockers, sr * 8 + f))
      break;
  }

  return attacks;
}

#ifndef USE_DUAL_HQ

BitBoard SetPieceLayoutOccupancy(int idx, int bits, BitBoard attacks) {
  BitBoard occupany = 0;

  for (int i = 0; i < bits; i++) {
    int sq = PopLSB(&attacks);

    if (idx & (1 << i))
      occupany |= (1ULL << sq);
  }

  return occupany;
}

uint64_t FindMagicNumber(int sq, int n, int isBishop) {
  int numOccupancies = 1 << n;

  BitBoard occupancies[4096];
  BitBoard attacks[4096];
  BitBoard usedAttacks[4096];

  BitBoard mask = isBishop ? MAGICS[sq].bishop.mask : MAGICS[sq].rook.mask;

  for (int i = 0; i < numOccupancies; i++) {
    occupancies[i] = SetPieceLayoutOccupancy(i, n, mask);
    attacks[i]     = isBishop ? GetBishopAttacksOTF(sq, occupancies[i]) : GetRookAttacksOTF(sq, occupancies[i]);
  }

  for (int count = 0; count < 10000000; count++) {
    uint64_t magic = RandomMagic();

    if (BitCount((mask * magic) & 0xFF00000000000000) < 6)
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

// Magics are searched for offline (see FindMagicNumber) and baked in so that
// startup does not pay for the search. Correctness is covered by the perft
// suite, which exercises every slider lookup.
void InitBishopMagics() {
  for (int i = 0; i < 64; i++)
    MAGICS[i].bishop.magic = BISHOP_MAGIC_NUMBERS[i];
}

void InitRookMagics() {
  for (int i = 0; i < 64; i++)
    MAGICS[i].rook.magic = ROOK_MAGIC_NUMBERS[i];
}

void InitBishopAttacks() {
  BitBoard* table = BISHOP_TABLE;

  for (int sq = 0; sq < 64; sq++) {
    Magic* m = &MAGICS[sq].bishop;
    int bits = BISHOP_RELEVANT_BITS[sq];
    int n    = (1 << bits);

    m->shift   = 64 - bits;
    m->attacks = table;

    for (int i = 0; i < n; i++) {
      BitBoard occupancy = SetPieceLayoutOccupancy(i, bits, m->mask);

      table[(occupancy * m->magic) >> m->shift] = GetBishopAttacksOTF(sq, occupancy);
    }

    table += n;
  }
}

void InitRookAttacks() {
  BitBoard* table = ROOK_TABLE;

  for (int sq = 0; sq < 64; sq++) {
    Magic* m = &MAGICS[sq].rook;
    int bits = ROOK_RELEVANT_BITS[sq];
    int n    = (1 << bits);

    m->shift   = 64 - bits;
    m->attacks = table;

    for (int i = 0; i < n; i++) {
      BitBoard occupancy = SetPieceLayoutOccupancy(i, bits, m->mask);

      table[(occupancy * m->magic) >> m->shift] = GetRookAttacksOTF(sq, occupancy);
    }

    table += n;
  }
}

#endif

#ifdef USE_DUAL_HQ

DualMagic DUAL_MAGICS[64] ALIGN;

// Rook attacks along a rank, indexed by the rook's file and the six inner bits
// of the rank's occupancy. The two edge squares never change the attack set,
// so they are left out of the index.
static uint8_t RANK_ATTACKS[8][64] ALIGN;

// Every square of the ray leaving sq, out to the edge of the board. Unlike a
// magic mask this keeps the edge squares, which hyperbola quintessence needs.
static BitBoard RayMask(int sq, int dr, int df) {
  BitBoard mask = 0;

  for (int r = Rank(sq) + dr, f = File(sq) + df; r >= 0 && r <= 7 && f >= 0 && f <= 7; r += dr, f += df)
    mask |= 1ULL << (r * 8 + f);

  return mask;
}

void InitDualMagics() {
  for (int f = 0; f < 8; f++)
    for (int occ = 0; occ < 64; occ++)
      RANK_ATTACKS[f][occ] = (uint8_t) GetRookAttacksOTF(f, (BitBoard) occ << 1);

  for (int sq = 0; sq < 64; sq++) {
    DualMagic* m = &DUAL_MAGICS[sq];

    m->maskFile     = RayMask(sq, 1, 0) | RayMask(sq, -1, 0);
    m->maskDiag     = RayMask(sq, 1, 1) | RayMask(sq, -1, -1);
    m->maskNone     = 0;
    m->maskAntiDiag = RayMask(sq, 1, -1) | RayMask(sq, -1, 1);

    // The subtrahends that carry a borrow along the ray, in board order and in
    // the byte reversed order the high lanes see.
    m->r  = (1ULL << sq) * 2;
    m->rr = (1ULL << (63 - sq)) * 2;

    m->rankAttacks = RANK_ATTACKS[File(sq)];
    m->shift       = 8 * Rank(sq);
  }
}

#endif

void InitSliderRays() {
  for (int sq = 0; sq < 64; sq++) {
    BISHOP_RAYS[sq] = GetBishopAttacksOTF(sq, 0);
    ROOK_RAYS[sq]   = GetRookAttacksOTF(sq, 0);
  }
}

void InitAttacks() {
  InitBetweenSquares();
  InitPinnedMovementSquares();

  InitPawnAttacks();
  InitKnightAttacks();
  InitKingAttacks();
  InitSliderRays();

#ifdef USE_DUAL_HQ
  InitDualMagics();
#else
  InitBishopMasks();
  InitRookMasks();

  InitBishopMagics();
  InitRookMagics();

  InitBishopAttacks();
  InitRookAttacks();
#endif
}


