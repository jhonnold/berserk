// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2026 Jay Honnold

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

typedef struct {
  int castling;
  Square ep;
  int fmr;
  Ply nullply;
  Key zobrist;
  Key pawnZobrist;
  Key nonPawnZobrist[2];
  BitBoard checkers;
  BitBoard pinned;
  BitBoard threatened;
  BitBoard threatenedBy[6];
  Piece capture;
} BoardHistory;

struct Board {
  // The below are in order of BoardHistory above for copies
  int castling;    // castling mask e.g. 1111 = KQkq, 1001 = Kq
  Square epSquare; // en passant square (a8 or 0 is not valid so that marks no
                   // active ep)
  int fmr;         // half move count for 50 move rule
  Ply nullply;     // distance from last nullmove

  Key zobrist;           // zobrist hash of the position
  Key pawnZobrist;       // pawn zobrist hash of the position (pawns + stm)
  Key nonPawnZobrist[2]; // per color non-pawn zobrist hash (pieces + stm)

  BitBoard checkers; // checking piece squares
  BitBoard pinned;   // pinned pieces

  BitBoard threatened; // opponent "threatening" these squares
  BitBoard threatenedBy[6];

  Color stm;   // side to move
  Color xstm;  // not side to move
  Ply histPly; // ply for historical state
  int moveNo;  // game move number
  int phase;   // efficiently updated phase for scaling

  uint64_t piecesCounts; // "material key" - pieces left on the board

  uint8_t squares[64];     // piece per square
  BitBoard occupancies[3]; // 0 - white pieces, 1 - black pieces, 2 - both
  BitBoard pieces[12];     // individual piece data

  int cr[4];
  int castlingRights[64];

  BoardHistory history[MAX_SEARCH_PLY + 100];

  Accumulator* accumulators;
  AccumulatorKingState* refreshTable;
};

#define NO_PIECE 12

#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

#define MakePiece(pc, c)  (((pc) << 1) + (c))
#define TypeOf(pc)        ((pc) >> 1)
#define PieceBB(b, pc, c) ((b)->pieces[MakePiece(pc, (c))])
#define OccBB(b, c)       ((b)->occupancies[c])

#define File(sq)         ((sq) & 7)
#define Rank(sq)         ((sq) >> 3)
#define MakeSquare(r, f) ((r) * 8 + (f))
#define PieceCount(pc)   (1ull << ((pc) * 4))

void ParseFen(const char* fen, Board* board);
void BoardToFen(char* fen, Board* board);
void PrintBoard(Board* board);

Bool DoesMoveCheck(Move move, Board* board);

Bool IsFiftyMoveRule(Board* board);

INLINE Bool IsRepetition(Board* board, Ply ply) {
  int reps = 0, distance = Min(board->fmr, board->nullply);

  // Check as far back as the last non-reversible move
  for (int i = board->histPly - 4; i >= 0 && i >= board->histPly - distance; i -= 2) {
    if (board->history[i].zobrist == board->zobrist) {
      if (i > board->histPly - ply) // within our search tree
        return 1;

      reps++;
      if (reps == 2) // 3-fold before+including root
        return 1;
    }
  }

  return 0;
}

INLINE Bool IsMaterialDraw(Board* board) {
  switch (board->piecesCounts) {
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
      return 1;
    default: return 0;
  }
}

INLINE Bool IsDraw(Board* board, Ply ply) {
  return IsRepetition(board, ply) || IsMaterialDraw(board) || IsFiftyMoveRule(board);
}

void MakeNullMove(Board* board);
void UndoNullMove(Board* board);
void MakeMove(Move move, Board* board);
void MakeMoveUpdate(Move move, Board* board, Bool update);
void UndoMove(Move move, Board* board);
void UndoMoveUpdate(Move move, Board* board, Bool update);

Bool IsPseudoLegal(Move move, Board* board);
Bool IsLegal(Move move, Board* board);

void InitCuckoo(void);
Bool HasCycle(Board* board, Ply ply);

INLINE BitBoard OpponentsEasyCaptures(Board* board) {
  const Color stm       = board->stm;
  const BitBoard queens = PieceBB(board, QUEEN, stm);
  const BitBoard rooks  = queens | PieceBB(board, ROOK, stm);
  const BitBoard minors = rooks | PieceBB(board, BISHOP, stm) | PieceBB(board, KNIGHT, stm);

  const BitBoard pawnThreats  = board->threatenedBy[PAWN];
  const BitBoard minorThreats = pawnThreats | board->threatenedBy[KNIGHT] | board->threatenedBy[BISHOP];
  const BitBoard rookThreats  = minorThreats | board->threatenedBy[ROOK];

  return (queens & rookThreats) | (rooks & minorThreats) | (minors & pawnThreats);
}

INLINE Bool HasNonPawn(Board* board, const Color color) {
  return !!(OccBB(board, color) ^ PieceBB(board, KING, color) ^ PieceBB(board, PAWN, color));
}

#endif