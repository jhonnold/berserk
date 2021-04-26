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

#include <stdlib.h>

#include "bits.h"
#include "board.h"
#include "endgame.h"
#include "eval.h"
#include "util.h"

const uint8_t kpkResults[2 * 64 * 64 * 24 / 8] = {
#include "kpk.h"
};

const int WINNING_ENDGAME = 10000;

// Returns an offset for getting king's close
// and pushing to corner - It is always negative and
// is designed as an offset
int Push(Board* board, int ss) {
  int s = 0;

  int ssKingSq = lsb(board->pieces[KING[ss]]);
  int wsKingSq = lsb(board->pieces[KING[1 - ss]]);

  s -= distance(ssKingSq, wsKingSq) * 25;

  int wsKingRank = rank(wsKingSq) > 3 ? 7 - rank(wsKingSq) : rank(wsKingSq);
  s -= wsKingRank * 15;

  int wsKingFile = file(wsKingSq) > 3 ? 7 - file(wsKingSq) : file(wsKingSq);
  s -= wsKingFile * 15;

  return s;
}

int StaticMaterialScore(int side, Board* board) {
  int s = 0;
  for (int p = PAWN[side]; p <= QUEEN[side]; p += 2)
    s += bits(board->pieces[p]) * STATIC_MATERIAL_VALUE[PIECE_TYPE[p]];

  return s;
}

int EvaluateKXK(Board* board) {
  if (board->pieces[QUEEN_WHITE] | board->pieces[QUEEN_BLACK] | board->pieces[ROOK_WHITE] | board->pieces[ROOK_BLACK]) {
    int ss = (board->pieces[QUEEN_WHITE] | board->pieces[ROOK_WHITE]) ? WHITE : BLACK;
    return ss == board->side ? WINNING_ENDGAME + Push(board, ss) : -WINNING_ENDGAME - Push(board, ss);
  } else if (board->pieces[PAWN_WHITE] | board->pieces[PAWN_BLACK]) {
    int eval = WINNING_ENDGAME - 1000; // don't want the engine to not queen
    int ss = board->pieces[PAWN_WHITE] ? WHITE : BLACK;

    int ssKingSq = lsb(board->pieces[KING[ss]]);
    int wsKingSq = lsb(board->pieces[KING[1 - ss]]);
    int pawnSq = lsb(board->pieces[PAWN[ss]]);

    if (KPKDraw(ss, ssKingSq, wsKingSq, pawnSq, board->side))
      return 0;

    // offset eval based on how far king is away from queen sq/pawn distance from queen sq
    int queenSq = ss == WHITE ? file(pawnSq) : file(pawnSq) + A1;
    eval -= 10 * distance(pawnSq, queenSq);
    eval -= distance(ssKingSq, queenSq);

    return ss == board->side ? eval : -eval;
  }

  return UNKNOWN;
}

int EvaluateMaterialOnlyEndgame(Board* board) {
  int whiteMaterial = StaticMaterialScore(WHITE, board);
  int whitePieceCount = bits(board->occupancies[WHITE]);

  int blackMaterial = StaticMaterialScore(BLACK, board);
  int blackPieceCount = bits(board->occupancies[BLACK]);

  int ss = whiteMaterial >= blackMaterial ? WHITE : BLACK;

  if (!whiteMaterial || !blackMaterial) {
    return ss == board->side ? WINNING_ENDGAME + Push(board, ss) : -WINNING_ENDGAME - Push(board, ss);
  } else {
    int ssMaterial = ss == WHITE ? whiteMaterial : blackMaterial;
    int wsMaterial = ss == WHITE ? blackMaterial : whiteMaterial;
    int ssPieceCount = ss == WHITE ? whitePieceCount : blackPieceCount;
    int wsPieceCount = ss == WHITE ? blackPieceCount : whitePieceCount;

    int materialDiff = ssMaterial - wsMaterial;
    int eval = materialDiff + Push(board, ss);

    if (ssPieceCount <= wsPieceCount + 1 && materialDiff <= STATIC_MATERIAL_VALUE[BISHOP_TYPE]) {
      if (ssMaterial >= STATIC_MATERIAL_VALUE[ROOK_TYPE] + STATIC_MATERIAL_VALUE[BISHOP_TYPE])
        eval /= 4;
      else
        eval /= 8;
    }

    return ss == board->side ? eval : -eval;
  }
}

// The following KPK code is modified for my use from Cheng (as is the dataset)
uint8_t GetKPKBit(int bit) { return kpkResults[bit >> 3] & (1U << (bit & 7)); }

int KPKIndex(int ssKing, int wsKing, int p, int stm) {
  int file = file(p);
  int x = file > 3 ? 7 : 0;

  ssKing ^= x;
  wsKing ^= x;
  p ^= x;
  file ^= x;

  int pawn = (((p & 0x38) - 8) >> 1) | file;

  return ((unsigned)pawn << 13) | ((unsigned)stm << 12) | ((unsigned)wsKing << 6) | ((unsigned)ssKing);
}

int KPKDraw(int ss, int ssKing, int wsKing, int p, int stm) {
  int x = ss == WHITE ? 0 : 0x38;
  int idx = KPKIndex(ssKing ^ x, wsKing ^ x, p ^ x, stm ^ x);

  return GetKPKBit(idx);
}