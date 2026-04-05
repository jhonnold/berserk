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

#include <assert.h>
#include <stdlib.h>

#include "bits.h"
#include "board.h"
#include "endgame.h"
#include "eval.h"
#include "search.h"
#include "util.h"

const uint8_t kpkResults[2 * 64 * 64 * 24 / 8] = {
#include "kpk.h"
};

const int WINNING_ENDGAME = 10000;

// Returns an offset for getting king's close
// and pushing to corner
int Push(Board* board, int ss) {
  int s = 0;

  int ssKingSq = LSB(PieceBB(KING, ss));
  int wsKingSq = LSB(PieceBB(KING, 1 - ss));

  s -= Distance(ssKingSq, wsKingSq) * 25;

  int wsKingRank = Rank(wsKingSq) > 3 ? 7 - Rank(wsKingSq) : Rank(wsKingSq);
  s -= wsKingRank * 15;

  int wsKingFile = File(wsKingSq) > 3 ? 7 - File(wsKingSq) : File(wsKingSq);
  s -= wsKingFile * 15;

  return s;
}

int StaticMaterialScore(int side, Board* board) {
  int s = 0;
  for (int p = Piece(PAWN, side); p <= Piece(QUEEN, side); p += 2)
    s += BitCount(board->pieces[p]) * STATIC_MATERIAL_VALUE[PieceType(p)];

  return s;
}

int EvaluateKXK(Board* board) {
  if (PPieceBB(WHITE_QUEEN) | PPieceBB(BLACK_QUEEN) | PPieceBB(WHITE_ROOK) | PPieceBB(BLACK_ROOK)) {
    int ss = (PPieceBB(WHITE_QUEEN) | PPieceBB(WHITE_ROOK)) ? WHITE : BLACK;
    return ss == board->stm ? WINNING_ENDGAME + Push(board, ss) : -WINNING_ENDGAME - Push(board, ss);
  } else if (PPieceBB(WHITE_PAWN) | PPieceBB(BLACK_PAWN)) {
    int eval = WINNING_ENDGAME - 1000; // don't want the engine to not queen
    int ss = PPieceBB(WHITE_PAWN) ? WHITE : BLACK;

    int ssKingSq = LSB(PieceBB(KING, ss));
    int wsKingSq = LSB(PieceBB(KING, 1 - ss));
    int pawnSq = LSB(PieceBB(PAWN, ss));

    if (KPKDraw(ss, ssKingSq, wsKingSq, pawnSq, board->stm))
      return 0;

    // offset eval based on how far king is away from queen sq/pawn distance from queen sq
    int queenSq = ss == WHITE ? File(pawnSq) : File(pawnSq) + A1;
    eval -= 10 * Distance(pawnSq, queenSq);
    eval -= Distance(ssKingSq, queenSq);

    return ss == board->stm ? eval : -eval;
  }

  return UNKNOWN;
}

int EvaluateMaterialOnlyEndgame(Board* board) {
  int whiteMaterial = StaticMaterialScore(WHITE, board);
  int whitePieceCount = BitCount(OccBB(WHITE));

  int blackMaterial = StaticMaterialScore(BLACK, board);
  int blackPieceCount = BitCount(OccBB(BLACK));

  int ss = whiteMaterial >= blackMaterial ? WHITE : BLACK;

  if (!whiteMaterial || !blackMaterial) {
    return ss == board->stm ? WINNING_ENDGAME + Push(board, ss) : -WINNING_ENDGAME - Push(board, ss);
  } else {
    int ssMaterial = ss == WHITE ? whiteMaterial : blackMaterial;
    int wsMaterial = ss == WHITE ? blackMaterial : whiteMaterial;
    int ssPieceCount = ss == WHITE ? whitePieceCount : blackPieceCount;
    int wsPieceCount = ss == WHITE ? blackPieceCount : whitePieceCount;

    int materialDiff = ssMaterial - wsMaterial;
    int eval = materialDiff + Push(board, ss);

    if (ssPieceCount <= wsPieceCount + 1 && materialDiff <= STATIC_MATERIAL_VALUE[BISHOP]) {
      if (ssMaterial >= STATIC_MATERIAL_VALUE[ROOK] + STATIC_MATERIAL_VALUE[BISHOP])
        eval /= 4;
      else
        eval /= 8;
    }

    return ss == board->stm ? eval : -eval;
  }
}

// The following KPK code is modified for my use from Cheng (as is the dataset)
uint8_t GetKPKBit(uint32_t bit) { return (uint8_t)(kpkResults[bit >> 3] & (1U << (bit & 7))); }

uint32_t KPKIndex(int ssKing, int wsKing, int p, int stm) {
  int f = File(p);
  int x = f > 3 ? 7 : 0;

  ssKing ^= x;
  wsKing ^= x;
  p ^= x;
  f ^= x;

  uint32_t pawn = (((p & 0x38) - 8) >> 1) | f;

  return (uint32_t)ssKing | ((uint32_t)wsKing << 6) | ((uint32_t)stm << 12) | ((uint32_t)pawn << 13);
}

uint8_t KPKDraw(int ss, int ssKing, int wsKing, int p, int stm) {
  uint32_t x = (ss == WHITE) ? 0u : 0x38u;
  uint32_t idx = KPKIndex(ssKing ^ x, wsKing ^ x, p ^ x, ss ^ stm);

  return GetKPKBit(idx);
}
