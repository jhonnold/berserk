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

#include "threats.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../attacks.h"
#include "../board.h"

int8_t THREAT_WEIGHTS[N_THREATS * N_HIDDEN] ALIGN;

int32_t THREAT_PAIR[10][12];
uint16_t THREAT_OFFSET[10][64];
BitBoard THREAT_EMPTY[10][64];

BitBoard RAY_PASS[64][64];

static const int MAP[5][6] = {
  {0, 1, -1, 2, -1, -1}, // P
  {0, 1, 2, 3, 4, -1},   // N
  {0, 1, 2, 3, -1, -1},  // B
  {0, 1, 2, 3, -1, -1},  // R
  {0, 1, 2, 3, 4, -1},   // Q
};

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

void PieceThreats(Board* board, DirtyThreatList* list, int pc, int s, int add, int rays) {
  const BitBoard occ     = OccBB(BOTH);
  const BitBoard kings   = board->pieces[WHITE_KING] | board->pieces[BLACK_KING];
  const BitBoard queens  = board->pieces[WHITE_QUEEN] | board->pieces[BLACK_QUEEN];
  const BitBoard diagSl  = board->pieces[WHITE_BISHOP] | board->pieces[BLACK_BISHOP] | queens;
  const BitBoard orthoSl = board->pieces[WHITE_ROOK] | board->pieces[BLACK_ROOK] | queens;

  const BitBoard sliders = (GetBishopAttacks(s, occ) & diagSl) | (GetRookAttacks(s, occ) & orthoSl);

  if (PieceType(pc) != KING) {
    BitBoard out = PieceAttacks(pc, s, occ) & occ & ~kings & FeatureVictims(board, PieceType(pc));
    while (out) {
      const int to = PopLSB(&out);
      Push(list, pc, s, board->squares[to], to, add);
    }

    BitBoard in = sliders | (GetKnightAttacks(s) & (board->pieces[WHITE_KNIGHT] | board->pieces[BLACK_KNIGHT])) |
                  (GetPawnAttacks(s, WHITE) & board->pieces[BLACK_PAWN]) |
                  (GetPawnAttacks(s, BLACK) & board->pieces[WHITE_PAWN]);
    in &= FeatureAttackers(board, PieceType(pc));
    while (in) {
      const int from = PopLSB(&in);
      Push(list, board->squares[from], from, pc, s, add);
    }
  }

  BitBoard sl = rays ? sliders : 0;
  while (sl) {
    const int from      = PopLSB(&sl);
    const BitBoard past = RAY_PASS[from][s] & occ;
    if (!past)
      continue;

    const int to       = from < s ? LSB(past) : MSB(past);
    const int victim   = board->squares[to];
    const int attacker = board->squares[from];
    if (PieceType(victim) != KING && (PieceType(victim) != QUEEN || PieceType(attacker) == QUEEN))
      Push(list, attacker, from, victim, to, !add);
  }

  XrayThreats(board, list, pc, s, add, 1);
}

// The own sliders a slider on sq sees first along its lines: the pieces it x-rays through
INLINE BitBoard XrayFronts(Board* board, int pc, int sq, BitBoard occ) {
  const int c          = pc & 1;
  const BitBoard queen = board->pieces[Piece(QUEEN, c)];

  BitBoard fronts = 0;
  if (PieceType(pc) != ROOK)
    fronts |= GetBishopAttacks(sq, occ) & (board->pieces[Piece(BISHOP, c)] | queen);
  if (PieceType(pc) != BISHOP)
    fronts |= GetRookAttacks(sq, occ) & (board->pieces[Piece(ROOK, c)] | queen);
  return fronts;
}

void XrayThreats(Board* board, DirtyThreatList* list, int pc, int s, int add, int open) {
  const BitBoard occ     = OccBB(BOTH);
  const BitBoard queens  = board->pieces[WHITE_QUEEN] | board->pieces[BLACK_QUEEN];
  const BitBoard diagSl  = board->pieces[WHITE_BISHOP] | board->pieces[BLACK_BISHOP] | queens;
  const BitBoard orthoSl = board->pieces[WHITE_ROOK] | board->pieces[BLACK_ROOK] | queens;
  const int pt           = PieceType(pc);

  // pc's own x-rays
  if (pt >= BISHOP && pt <= QUEEN) {
    BitBoard fronts = XrayFronts(board, pc, s, occ);
    while (fronts) {
      const int f  = PopLSB(&fronts);
      const int to = NextPast(s, f, occ);
      if (to >= 0 && PieceType(board->squares[to]) != KING)
        PushXray(list, pc, s, board->squares[to], to, add);
    }
  }

  // sliders seeing s through one piece that lets them: they x-ray pc, or past s if empty
  const BitBoard diag = GetBishopAttacks(s, occ), ortho = GetRookAttacks(s, occ);
  BitBoard behind     = (GetBishopAttacks(s, occ ^ (diag & occ)) & ~diag & diagSl) |
                    (GetRookAttacks(s, occ ^ (ortho & occ)) & ~ortho & orthoSl);
  while (behind) {
    const int a        = PopLSB(&behind);
    const int attacker = board->squares[a];
    const int f        = LSB(BetweenSquares(a, s) & occ);
    if (!Fronts(attacker, board->squares[f], !!(diag & Bit(f))))
      continue;

    if (pt != KING)
      PushXray(list, attacker, a, pc, s, add);

    const int to = open ? NextPast(a, s, occ) : -1;
    if (to >= 0 && PieceType(board->squares[to]) != KING)
      PushXray(list, attacker, a, board->squares[to], to, !add);
  }

  // sliders seeing s directly: they x-ray through pc, or through the next piece if empty
  BitBoard direct = (diag & diagSl) | (ortho & orthoSl);
  while (direct) {
    const int a        = PopLSB(&direct);
    const int attacker = board->squares[a];
    const int isDiag   = !!(diag & Bit(a));
    const int v        = NextPast(a, s, occ);
    if (v < 0)
      continue;

    if (Fronts(attacker, pc, isDiag) && PieceType(board->squares[v]) != KING)
      PushXray(list, attacker, a, board->squares[v], v, add);

    if (open && Fronts(attacker, board->squares[v], isDiag)) {
      const int to = NextPast(a, v, occ);
      if (to >= 0 && PieceType(board->squares[to]) != KING)
        PushXray(list, attacker, a, board->squares[to], to, !add);
    }
  }
}

int ActiveThreats(Board* board, const int view, int* indices) {
  const BitBoard occ     = OccBB(BOTH);
  const BitBoard targets = occ & ~(PieceBB(KING, WHITE) | PieceBB(KING, BLACK));
  const int flip         = FeatureFlip(LSB(PieceBB(KING, view)), view);

  int n = 0;

  for (int pc = WHITE_PAWN; pc <= BLACK_QUEEN; pc++) {
    const BitBoard victims = targets & FeatureVictims(board, PieceType(pc));

    BitBoard attackers = board->pieces[pc];
    while (attackers) {
      const int from = PopLSB(&attackers);

      BitBoard t = PieceAttacks(pc, from, occ) & victims;
      while (t) {
        const int to  = PopLSB(&t);
        const int idx = ThreatIdx(pc, from, board->squares[to], to, view, flip);
        if (idx >= 0) {
          __builtin_prefetch(&THREAT_WEIGHTS[(size_t) idx * N_HIDDEN]);
          indices[n++] = idx;
        }
      }

      if (PieceType(pc) < BISHOP)
        continue;

      BitBoard fronts = XrayFronts(board, pc, from, occ);
      while (fronts) {
        const int to = NextPast(from, PopLSB(&fronts), occ);
        if (to < 0 || PieceType(board->squares[to]) == KING)
          continue;

        const int idx = XrayIdx(pc, from, board->squares[to], to, view, flip);
        if (idx >= 0) {
          __builtin_prefetch(&THREAT_WEIGHTS[(size_t) idx * N_HIDDEN]);
          indices[n++] = idx;
        }
      }
    }
  }

  return n;
}
