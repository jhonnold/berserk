// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2022 Jay Honnold

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

#include "endgame.h"

#include <assert.h>
#include <stdlib.h>

#include "bits.h"
#include "board.h"
#include "search.h"
#include "see.h"
#include "util.h"

const uint8_t kpkResults[2 * 64 * 64 * 24 / 8] = {
#include "kpk.h"
};

INLINE int PushTogether(int sq1, int sq2) { return 70 - Distance(sq1, sq2); }

INLINE int PushToEdge(int sq) {
  int rankDistance = min(Rank(sq), 7 - Rank(sq));
  int fileDistance = min(File(sq), 7 - File(sq));

  return 20 - (rankDistance * rankDistance) - (fileDistance * fileDistance);
}

INLINE int MaterialValue(Board* board, const int side) {
  int staticScore = 0;
  for (int piece = PAWN; piece <= QUEEN; piece++) staticScore += bits(PieceBB(piece, side)) * SEE_VALUE[piece];

  return staticScore;
}

INLINE int EvaluateKPK(Board* board, const int winningSide) {
  const int losingSide = winningSide ^ 1;
  int score = WINNING_ENDGAME + MaterialValue(board, winningSide);

  int winningKing = lsb(PieceBB(KING, winningSide));
  int losingKing = lsb(PieceBB(KING, losingSide));

  int pawn = lsb(PieceBB(PAWN, winningSide));

  if (KPKDraw(winningSide, winningKing, losingKing, pawn, board->stm)) return 0;

  score += winningSide == WHITE ? (7 - Rank(pawn)) : Rank(pawn);


  return winningSide == board->stm ? score : -score;
}

INLINE int EvaluateKXK(Board* board, const int winningSide) {
  const int losingSide = winningSide ^ 1;
  int score = WINNING_ENDGAME + MaterialValue(board, winningSide);

  int winningKing = lsb(PieceBB(KING, winningSide));
  int losingKing = lsb(PieceBB(KING, losingSide));

  score += PushTogether(winningKing, losingKing) + PushToEdge(losingKing);

  score = winningSide == board->stm ? score : -score;
  return min(TB_WIN_BOUND - 1, max(-TB_WIN_BOUND + 1, score));
}

INLINE int EvaluateKBNK(Board* board, const int winningSide) {
  const int losingSide = winningSide ^ 1;
  int score = WINNING_ENDGAME + MaterialValue(board, winningSide);

  int winningKing = lsb(PieceBB(KING, winningSide));
  int losingKing = lsb(PieceBB(KING, losingSide));

  score += PushTogether(winningKing, losingKing);

  if (DARK_SQS & PieceBB(BISHOP, winningSide)) {
    score += 50 * (7 - min(MDistance(losingKing, A1), MDistance(losingKing, H8)));
  } else {
    score += 50 * (7 - min(MDistance(losingKing, A8), MDistance(losingKing, H1)));
  }

  score = winningSide == board->stm ? score : -score;
  return min(TB_WIN_BOUND - 1, max(-TB_WIN_BOUND + 1, score));
}

int EvaluateKnownPositions(Board* board) {
  switch (board->piecesCounts) {
    // See IsMaterialDraw
    case 0x0:       // Kk
    case 0x100:     // KNk
    case 0x200:     // KNNk
    case 0x1000:    // Kkn
    case 0x2000:    // Kknn
    case 0x1100:    // KNkn
    case 0x10000:   // KBk
    case 0x100000:  // Kkb
    case 0x11000:   // KBkn
    case 0x100100:  // KNkb
    case 0x110000:  // KBkb
      return 0;
    case 0x1:  // KPk
      return EvaluateKPK(board, WHITE);
    case 0x10:  // Kpk
      return EvaluateKPK(board, BLACK);
    case 0x10100: // KBNk
      return EvaluateKBNK(board, WHITE);
    case 0x101000:
      return EvaluateKBNK(board, BLACK);
    default:
      break;
  }

  if (bits(OccBB(BLACK)) == 1)
    return EvaluateKXK(board, WHITE);
  else if (bits(OccBB(WHITE)) == 1)
    return EvaluateKXK(board, BLACK);

  return UNKNOWN;
}

// The following KPK code is modified for my use from Cheng (as is the dataset)
uint8_t GetKPKBit(uint32_t bit) { return (uint8_t)(kpkResults[bit >> 3] & (1U << (bit & 7))); }

uint32_t KPKIndex(int winningKing, int losingKing, int pawn, int stm) {
  int file = File(pawn);
  int x = file > 3 ? 7 : 0;

  winningKing ^= x;
  losingKing ^= x;
  pawn ^= x;
  file ^= x;

  uint32_t p = (((pawn & 0x38) - 8) >> 1) | file;

  return (uint32_t)winningKing | ((uint32_t)losingKing << 6) | ((uint32_t)stm << 12) | ((uint32_t)p << 13);
}

uint8_t KPKDraw(int winningSide, int winningKing, int losingKing, int pawn, int stm) {
  uint32_t x = (winningSide == WHITE) ? 0u : 0x38u;
  uint32_t idx = KPKIndex(winningKing ^ x, losingKing ^ x, pawn ^ x, winningSide ^ stm);

  return GetKPKBit(idx);
}