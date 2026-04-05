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

#ifndef BOARD_H
#define BOARD_H

#include "types.h"
#include "util.h"

#define NO_PIECE 12

#define Piece(pc, c)   (((pc) << 1) + c)
#define PieceType(pc)  ((pc) >> 1)
#define PPieceBB(pc)   (board->pieces[pc])
#define PieceBB(pc, c) (board->pieces[Piece(pc, (c))])
#define OccBB(c)       (board->occupancies[c])

extern const int MIRROR[];
extern Score PSQT[12][2][64];

#define File(sq)        ((sq) &7)
#define Rank(sq)        ((sq) >> 3)
#define Sq(r, f)        ((r) *8 + (f))
#define Distance(a, b)  Max(abs(Rank(a) - Rank(b)), abs(File(a) - File(b)))
#define MDistance(a, b) (abs(Rank(a) - Rank(b)) + abs(File(a) - File(b)))
#define PieceCount(pc)  (1ull << (pc * 4))

void ClearBoard(Board* board);
void ParseFen(char* fen, Board* board);
void BoardToFen(char* fen, Board* board);
void PrintBoard(Board* board);

void SetSpecialPieces(Board* board);
void SetThreats(Board* board);

int DoesMoveCheck(Move move, Board* board);

int IsDraw(Board* board, int ply);
int IsRepetition(Board* board, int ply);
int IsMaterialDraw(Board* board);
int IsFiftyMoveRule(Board* board);

void MakeNullMove(Board* board);
void UndoNullMove(Board* board);
void MakeMove(Move move, Board* board);
void MakeMoveUpdate(Move move, Board* board, int update);
void UndoMove(Move move, Board* board);

int IsPseudoLegal(Move move, Board* board);
int IsLegal(Move move, Board* board);

void InitCuckoo();
int HasCycle(Board* board, int ply);

INLINE BitBoard OpponentsEasyCaptures(Board* board) {
  const int stm         = board->stm;
  const BitBoard queens = PieceBB(QUEEN, stm);
  const BitBoard rooks  = queens | PieceBB(ROOK, stm);
  const BitBoard minors = rooks | PieceBB(BISHOP, stm) | PieceBB(KNIGHT, stm);

  const BitBoard pawnThreats  = board->threatenedBy[PAWN];
  const BitBoard minorThreats = pawnThreats | board->threatenedBy[KNIGHT] | board->threatenedBy[BISHOP];
  const BitBoard rookThreats  = minorThreats | board->threatenedBy[ROOK];

  return (queens & rookThreats) | (rooks & minorThreats) | (minors & pawnThreats);
}

INLINE int HasNonPawn(Board* board, const int color) {
  return !!(OccBB(color) ^ PieceBB(KING, color) ^ PieceBB(PAWN, color));
}

#endif