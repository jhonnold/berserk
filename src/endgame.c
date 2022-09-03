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

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_CAMEL
#include "incbin.h"

INCBIN(KPK, "bitbases/KPvK.bb");
INCBIN(KBPK, "bitbases/KBPvK.bb");

INLINE int PushTogether(int sq1, int sq2) {
  return 70 - Distance(sq1, sq2);
}

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

// The following KPK code is modified for my use from Cheng (as is the dataset)
INLINE uint32_t KPKIndex(int winningKing, int losingKing, int pawn, int stm) {
  int file = File(pawn);
  int x    = file > 3 ? 7 : 0;

  winningKing ^= x;
  losingKing ^= x;
  pawn ^= x;
  file ^= x;

  uint32_t p = (((pawn & 0x38) - 8) >> 1) | file;

  return (uint32_t) winningKing | ((uint32_t) losingKing << 6) | ((uint32_t) stm << 12) | ((uint32_t) p << 13);
}

INLINE uint8_t KPKDraw(int winningSide, int winningKing, int losingKing, int pawn, int stm) {
  uint32_t x   = (winningSide == WHITE) ? 0 : 56;
  uint32_t idx = KPKIndex(winningKing ^ x, losingKing ^ x, pawn ^ x, winningSide ^ stm);

  return (uint8_t) (KPKData[idx >> 3] & (1U << (idx & 7)));
}

INLINE int EvaluateKPK(Board* board, const int winningSide) {
  const int losingSide = winningSide ^ 1;
  int score            = WINNING_ENDGAME + MaterialValue(board, winningSide);

  int winningKing = lsb(PieceBB(KING, winningSide));
  int losingKing  = lsb(PieceBB(KING, losingSide));
  int pawn        = winningSide == WHITE ? lsb(PieceBB(PAWN, WHITE)) : msb(PieceBB(PAWN, BLACK));

  if (KPKDraw(winningSide, winningKing, losingKing, pawn, board->stm)) return 0;

  score += winningSide == WHITE ? (7 - Rank(pawn)) : Rank(pawn);

  return winningSide == board->stm ? score : -score;
}

INLINE int EvaluateKXK(Board* board, const int winningSide) {
  const int losingSide = winningSide ^ 1;
  int score            = WINNING_ENDGAME + MaterialValue(board, winningSide);

  int winningKing = lsb(PieceBB(KING, winningSide));
  int losingKing  = lsb(PieceBB(KING, losingSide));

  score += PushTogether(winningKing, losingKing) + PushToEdge(losingKing);

  score = winningSide == board->stm ? score : -score;
  return min(TB_WIN_BOUND - 1, max(-TB_WIN_BOUND + 1, score));
}

INLINE int EvaluateKBNK(Board* board, const int winningSide) {
  const int losingSide = winningSide ^ 1;
  int score            = WINNING_ENDGAME + MaterialValue(board, winningSide);

  int winningKing = lsb(PieceBB(KING, winningSide));
  int losingKing  = lsb(PieceBB(KING, losingSide));

  score += PushTogether(winningKing, losingKing);

  if (DARK_SQS & PieceBB(BISHOP, winningSide)) {
    score += 50 * (7 - min(MDistance(losingKing, A1), MDistance(losingKing, H8)));
  } else {
    score += 50 * (7 - min(MDistance(losingKing, A8), MDistance(losingKing, H1)));
  }

  score = winningSide == board->stm ? score : -score;
  return min(TB_WIN_BOUND - 1, max(-TB_WIN_BOUND + 1, score));
}

INLINE uint32_t KBPKIndex(int wking, int lking, int bishop, int pawn, int stm) {
  int file = File(pawn);
  int x    = file > 3 ? 7 : 0;

  wking ^= x, lking ^= x, bishop ^= x, pawn ^= x;

  uint32_t p = (pawn >> 3) - 1;
  uint32_t b = bishop >> 1;

  return (uint32_t) wking | ((uint32_t) lking << 6) | (b << 12) | ((uint32_t) stm << 17) | (p << 18);
}

INLINE uint8_t KBPKDraw(int winningSide, int winningKing, int losingKing, int bishop, int pawn, int stm) {
  int x        = winningSide == WHITE ? 0 : 56;
  uint32_t idx = KBPKIndex(winningKing ^ x, losingKing ^ x, bishop ^ x, pawn ^ x, winningSide ^ stm);

  return !(uint8_t) (KBPKData[idx >> 3] & (1U << (idx & 7)));
}

INLINE int EvaluateKBPK(Board* board, const int winningSide) {
  const int losingSide = winningSide ^ 1;

  int pawn        = winningSide == WHITE ? lsb(PieceBB(PAWN, WHITE)) : msb(PieceBB(PAWN, BLACK));
  int promotionSq = winningSide == WHITE ? File(pawn) : A1 + File(pawn);
  int darkPromoSq = !!getBit(DARK_SQS, promotionSq);
  int darkBishop  = !!(DARK_SQS & PieceBB(BISHOP, winningSide));

  uint8_t files = PawnFiles(PieceBB(PAWN, winningSide));

  // "Winning" if not a rook pawn or have right bishop
  if ((files != 0x01 && files != 0x80) || darkPromoSq == darkBishop) {
    int score = WINNING_ENDGAME + MaterialValue(board, winningSide);

    score += winningSide == WHITE ? (7 - Rank(pawn)) : Rank(pawn);
    score = winningSide == board->stm ? score : -score;

    return min(TB_WIN_BOUND - 1, max(-TB_WIN_BOUND + 1, score));
  }

  // Utilize bitbase for everything else
  int winningKing = lsb(PieceBB(KING, winningSide));
  int losingKing  = lsb(PieceBB(KING, losingSide));
  int bishop      = lsb(PieceBB(BISHOP, winningSide));

  if (KBPKDraw(winningSide, winningKing, losingKing, bishop, pawn, board->stm)) return 0;

  int score = WINNING_ENDGAME + MaterialValue(board, winningSide);

  score += winningSide == WHITE ? (7 - Rank(pawn)) : Rank(pawn);
  score = winningSide == board->stm ? score : -score;

  return min(TB_WIN_BOUND - 1, max(-TB_WIN_BOUND + 1, score));
}

int EvaluateKnownPositions(Board* board) {
  switch (board->piecesCounts) {
    // See IsMaterialDraw
    case 0x0:      // Kk
    case 0x100:    // KNk
    case 0x200:    // KNNk
    case 0x1000:   // Kkn
    case 0x2000:   // Kknn
    case 0x1100:   // KNkn
    case 0x10000:  // KBk
    case 0x100000: // Kkb
    case 0x11000:  // KBkn
    case 0x100100: // KNkb
    case 0x110000: // KBkb
      return 0;
    case 0x1: // KPk
      return EvaluateKPK(board, WHITE);
    case 0x10: // Kkp
      return EvaluateKPK(board, BLACK);
    case 0x10100: // KBNk
      return EvaluateKBNK(board, WHITE);
    case 0x101000: // Kkbn
      return EvaluateKBNK(board, BLACK);
    default: break;
  }

  if (bits(OccBB(BLACK)) == 1) {
    // KBPK
    if ((board->piecesCounts ^ 0x10000) <= 0xF) return EvaluateKBPK(board, WHITE);

    // stacked rook pawns
    if (board->piecesCounts <= 0xF) {
      uint8_t files = PawnFiles(PieceBB(PAWN, WHITE));
      if (files == 0x1 || files == 0x80) return EvaluateKPK(board, WHITE);
    }

    return EvaluateKXK(board, WHITE);
  } else if (bits(OccBB(WHITE)) == 1) {
    // Kkbp
    if ((board->piecesCounts ^ 0x100000) <= 0xF0) return EvaluateKBPK(board, BLACK);

    // stacked rook pawns
    if (board->piecesCounts <= 0xF0) {
      uint8_t files = PawnFiles(PieceBB(PAWN, BLACK));
      if (files == 0x1 || files == 0x80) return EvaluateKPK(board, BLACK);
    }

    return EvaluateKXK(board, BLACK);
  }

  return UNKNOWN;
}
