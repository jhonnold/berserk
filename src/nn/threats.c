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

// Threat inputs: one feature per attack of a non-king piece on a non-king piece,
// indexed by (attacker, from, victim, to) in the perspective's frame. The trainer's
// `crates/train/src/threats.rs` builds the same layout; docs/INPUTS.md describes it.
//
// Features always active together with another one are dropped: P->B and P->Q (the
// bishop or queen attacks back), B->Q and R->Q (the queen sees the same line), and one
// direction of each mutual same-type pair. That leaves 60,144.

#include "threats.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../attacks.h"
#include "../board.h"
#include "../move.h"
#include "../movegen.h"

int8_t THREAT_WEIGHTS[N_THREATS * N_HIDDEN] ALIGN;

int32_t THREAT_PAIR[10][12];
uint16_t THREAT_OFFSET[10][64];
BitBoard THREAT_EMPTY[10][64];

// The squares beyond `s` on the line from `a` through `s`.
static BitBoard RAY_PASS[64][64];

// Victim class per (attacker type, victim type), -1 where the pair is dropped.
static const int MAP[5][6] = {
  {0, 1, -1, 2, -1, -1}, // P
  {0, 1, 2, 3, 4, -1},   // N
  {0, 1, 2, 3, -1, -1},  // B
  {0, 1, 2, 3, -1, -1},  // R
  {0, 1, 2, 3, 4, -1},   // Q
};

// Victim classes per attacker type, times the two victim colours.
static const int TARGETS[5] = {6, 10, 8, 8, 10};

static BitBoard EmptyAttacks(int colour, int pt, int sq) {
  switch (pt) {
    case PAWN: return (sq >= 8 && sq < 56) ? GetPawnAttacks(sq, colour) : 0;
    case KNIGHT: return GetKnightAttacks(sq);
    case BISHOP: return GetBishopAttacks(sq, 0);
    case ROOK: return GetRookAttacks(sq, 0);
    default: return GetQueenAttacks(sq, 0);
  }
}

void InitThreats() {
  int next = 0;

  for (int colour = 0; colour < 2; colour++) {
    for (int pt = PAWN; pt <= QUEEN; pt++) {
      const int block = 5 * colour + pt;

      int pairs = 0;
      for (int sq = 0; sq < 64; sq++) {
        // Relative white plays towards rank 8, as white does in absolute terms.
        THREAT_EMPTY[block][sq]  = EmptyAttacks(colour, pt, sq);
        THREAT_OFFSET[block][sq] = pairs;
        pairs += BitCount(THREAT_EMPTY[block][sq]);
      }

      for (int vc = 0; vc < 2; vc++) {
        for (int vt = PAWN; vt <= KING; vt++) {
          const int class = MAP[pt][vt];
          if (class < 0) {
            THREAT_PAIR[block][6 * vc + vt] = -1;
            continue;
          }

          const int first                 = next + (vc * (TARGETS[pt] / 2) + class) * pairs;
          const int mutual                = pt == vt && (pt != PAWN || colour != vc);
          THREAT_PAIR[block][6 * vc + vt] = (first << 1) | mutual;
        }
      }

      next += TARGETS[pt] * pairs;
    }
  }

  if (next != N_THREATS) {
    fprintf(stderr, "threat layout has %d features, expected %d\n", next, N_THREATS);
    exit(1);
  }

  for (int a = 0; a < 64; a++) {
    for (int s = 0; s < 64; s++) {
      RAY_PASS[a][s] = 0;
      if (a == s)
        continue;

      const int dr = (s >> 3) - (a >> 3), df = (s & 7) - (a & 7);
      if (dr && df && abs(dr) != abs(df))
        continue;

      const int stepR = (dr > 0) - (dr < 0), stepF = (df > 0) - (df < 0);
      for (int r = (s >> 3) + stepR, f = (s & 7) + stepF; r >= 0 && r < 8 && f >= 0 && f < 8; r += stepR, f += stepF)
        RAY_PASS[a][s] |= Bit(8 * r + f);
    }
  }
}

// A copy of the pieces that the move's steps can be played on, so each step sees the
// board exactly as it stands between the previous step and the next.
typedef struct {
  BitBoard pcs[12];
  BitBoard occ;
  uint8_t squares[64];
} ThreatBoard;

typedef struct {
  uint32_t* dirty;
  int n;
} DirtyList;

INLINE void Push(DirtyList* list, int attacker, int from, int victim, int to, int add) {
  list->dirty[list->n++] = from | (to << 8) | (attacker << 16) | (victim << 20) | ((uint32_t) add << 31);
}

INLINE BitBoard PieceAttacks(int pc, int sq, BitBoard occ) {
  switch (PieceType(pc)) {
    case PAWN: return GetPawnAttacks(sq, pc & 1);
    case KNIGHT: return GetKnightAttacks(sq);
    case BISHOP: return GetBishopAttacks(sq, occ);
    case ROOK: return GetRookAttacks(sq, occ);
    case QUEEN: return GetQueenAttacks(sq, occ);
    default: return 0;
  }
}

// Every threat involving `pc` on `s`, added or removed. `tb` must hold `pc` on `s`. With
// `rays`, also the attacks that `s` blocks for sliders seeing it, with the opposite sign:
// a piece arriving on `s` cuts them, one leaving opens them.
static void PieceThreats(ThreatBoard* tb, DirtyList* list, int pc, int s, int add, int rays) {
  const BitBoard occ     = tb->occ;
  const BitBoard kings   = tb->pcs[WHITE_KING] | tb->pcs[BLACK_KING];
  const BitBoard queens  = tb->pcs[WHITE_QUEEN] | tb->pcs[BLACK_QUEEN];
  const BitBoard diagSl  = tb->pcs[WHITE_BISHOP] | tb->pcs[BLACK_BISHOP] | queens;
  const BitBoard orthoSl = tb->pcs[WHITE_ROOK] | tb->pcs[BLACK_ROOK] | queens;

  const BitBoard sliders = (GetBishopAttacks(s, occ) & diagSl) | (GetRookAttacks(s, occ) & orthoSl);

  if (PieceType(pc) != KING) {
    BitBoard out = PieceAttacks(pc, s, occ) & occ & ~kings;
    while (out) {
      const int to = PopLSB(&out);
      Push(list, pc, s, tb->squares[to], to, add);
    }

    BitBoard in = sliders | (GetKnightAttacks(s) & (tb->pcs[WHITE_KNIGHT] | tb->pcs[BLACK_KNIGHT])) |
                  (GetPawnAttacks(s, WHITE) & tb->pcs[BLACK_PAWN]) | (GetPawnAttacks(s, BLACK) & tb->pcs[WHITE_PAWN]);
    while (in) {
      const int from = PopLSB(&in);
      Push(list, tb->squares[from], from, pc, s, add);
    }
  }

  if (rays) {
    BitBoard sl = sliders;
    while (sl) {
      const int from      = PopLSB(&sl);
      const BitBoard past = RAY_PASS[from][s] & occ;
      if (!past)
        continue;

      const int to     = from < s ? LSB(past) : MSB(past);
      const int victim = tb->squares[to];
      if (PieceType(victim) != KING)
        Push(list, tb->squares[from], from, victim, to, !add);
    }
  }
}

INLINE void Remove(ThreatBoard* tb, DirtyList* list, int pc, int s) {
  PieceThreats(tb, list, pc, s, SUB, 1);
  FlipBit(tb->pcs[pc], s);
  FlipBit(tb->occ, s);
  tb->squares[s] = NO_PIECE;
}

INLINE void Add(ThreatBoard* tb, DirtyList* list, int pc, int s) {
  FlipBit(tb->pcs[pc], s);
  FlipBit(tb->occ, s);
  tb->squares[s] = pc;
  PieceThreats(tb, list, pc, s, ADD, 1);
}

// Occupancy does not change, so no ray is cut or opened; sliders on `s` are just attackers.
INLINE void Swap(ThreatBoard* tb, DirtyList* list, int old, int new, int s) {
  PieceThreats(tb, list, old, s, SUB, 0);
  FlipBit(tb->pcs[old], s);
  FlipBit(tb->pcs[new], s);
  tb->squares[s] = new;
  PieceThreats(tb, list, new, s, ADD, 0);
}

int ThreatsForMove(Board* board, Move move, int captured, uint32_t* dirty) {
  ThreatBoard tb[1];
  memcpy(tb->pcs, board->pieces, sizeof(tb->pcs));
  memcpy(tb->squares, board->squares, sizeof(tb->squares));
  tb->occ = OccBB(BOTH);

  DirtyList list = {dirty, 0};

  const int from  = From(move);
  const int to    = To(move);
  const int piece = Moving(move);
  const int stm   = piece & 1;

  if (IsCas(move)) {
    const int rookFrom = board->cr[CASTLING_ROOK[to]];
    const int rookTo   = CASTLE_ROOK_DEST[to];
    const int rook     = Piece(ROOK, stm);

    Remove(tb, &list, piece, from);
    Remove(tb, &list, rook, rookFrom);
    Add(tb, &list, piece, to);
    Add(tb, &list, rook, rookTo);
  } else {
    const int placed = IsPromo(move) ? PromoPiece(move, stm) : piece;

    Remove(tb, &list, piece, from);

    if (IsEP(move)) {
      Remove(tb, &list, captured, to - PawnDir(stm));
      Add(tb, &list, placed, to);
    } else if (IsCap(move)) {
      Swap(tb, &list, captured, placed, to);
    } else {
      Add(tb, &list, placed, to);
    }
  }

  return list.n;
}

int ActiveThreats(Board* board, const int view, int* indices) {
  const BitBoard occ   = OccBB(BOTH);
  const BitBoard kings = PieceBB(KING, WHITE) | PieceBB(KING, BLACK);
  const int flip       = ThreatFlip(LSB(PieceBB(KING, view)), view);

  int n = 0;

  BitBoard attackers = occ & ~kings;
  while (attackers) {
    const int from = PopLSB(&attackers);
    const int pc   = board->squares[from];

    BitBoard targets = PieceAttacks(pc, from, occ) & occ & ~kings;
    while (targets) {
      const int to  = PopLSB(&targets);
      const int idx = ThreatIdx(pc, from, board->squares[to], to, view, flip);
      if (idx >= 0)
        indices[n++] = idx;
    }
  }

  return n;
}
