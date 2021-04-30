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

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "transposition.h"
#include "types.h"
#include "zobrist.h"

const BitBoard EMPTY = 0ULL;

const int PAWN[] = {PAWN_WHITE, PAWN_BLACK};
const int KNIGHT[] = {KNIGHT_WHITE, KNIGHT_BLACK};
const int BISHOP[] = {BISHOP_WHITE, BISHOP_BLACK};
const int ROOK[] = {ROOK_WHITE, ROOK_BLACK};
const int QUEEN[] = {QUEEN_WHITE, QUEEN_BLACK};
const int KING[] = {KING_WHITE, KING_BLACK};

// way to identify pieces are the same "type" (i.e. WHITE_PAWN and BLACK_PAWN)
const int PIECE_TYPE[13] = {PAWN_TYPE, PAWN_TYPE,  KNIGHT_TYPE, KNIGHT_TYPE, BISHOP_TYPE, BISHOP_TYPE, ROOK_TYPE,
                            ROOK_TYPE, QUEEN_TYPE, QUEEN_TYPE,  KING_TYPE,   KING_TYPE,   NO_PIECE / 2};

// clang-format off
// castling rights mask for start/end squares
const int CASTLING_RIGHTS[] = {
  14, 15, 15, 15, 12, 15, 15, 13, 
  15, 15, 15, 15, 15, 15, 15, 15, 
  15, 15, 15, 15, 15, 15, 15, 15, 
  15, 15, 15, 15, 15, 15, 15, 15, 
  15, 15, 15, 15, 15, 15, 15, 15, 
  15, 15, 15, 15, 15, 15, 15, 15, 
  15, 15, 15, 15, 15, 15, 15, 15, 
  11, 15, 15, 15, 3,  15, 15,  7
};

// square reflection table
const int MIRROR[] = {
  56, 57, 58, 59, 60, 61, 62, 63, 
  48, 49, 50, 51, 52, 53, 54, 55, 
  40, 41, 42, 43, 44, 45, 46, 47, 
  32, 33, 34, 35, 36, 37, 38, 39, 
  24, 25, 26, 27, 28, 29, 30, 31, 
  16, 17, 18, 19, 20, 21, 22, 23, 
   8,  9, 10, 11, 12, 13, 14, 15, 
   0,  1,  2,  3,  4,  5,  6,  7
};
// clang-format on

// piece count key bit mask idx
const uint64_t PIECE_COUNT_IDX[] = {1ULL << 0,  1ULL << 4,  1ULL << 8,  1ULL << 12, 1ULL << 16,
                                    1ULL << 20, 1ULL << 24, 1ULL << 28, 1ULL << 32, 1ULL << 36};

const uint64_t NON_PAWN_PIECE_MASK[] = {0x0F0F0F0F00, 0xF0F0F0F000};

// reset the board to an empty state
void ClearBoard(Board* board) {
  memset(board->pieces, EMPTY, sizeof(board->pieces));
  memset(board->occupancies, EMPTY, sizeof(board->occupancies));
  memset(board->zobristHistory, EMPTY, sizeof(board->zobristHistory));
  memset(board->castlingHistory, EMPTY, sizeof(board->castlingHistory));
  memset(board->captureHistory, EMPTY, sizeof(board->captureHistory));
  memset(board->halfMoveHistory, 0, sizeof(board->halfMoveHistory));

  for (int i = 0; i < 64; i++)
    board->squares[i] = NO_PIECE;

  board->piecesCounts = 0ULL;
  board->zobrist = 0ULL;
  board->pawnHash = 0ULL;

  board->mat = makeScore(0, 0);

  board->side = WHITE;
  board->xside = BLACK;

  board->epSquare = 0;
  board->castling = 0;
  board->moveNo = 0;
  board->halfMove = 0;
}

void ParseFen(char* fen, Board* board) {
  ClearBoard(board);

  for (int i = 0; i < 64; i++) {
    if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z')) {
      int piece = CHAR_TO_PIECE[(int)*fen];
      setBit(board->pieces[piece], i);
      board->squares[i] = piece;

      if (piece & 1) {
        board->mat -= PSQT[piece][i];
      } else {
        board->mat += PSQT[piece][i];
      }

      if (PIECE_TYPE[piece] == PAWN_TYPE)
        board->pawnHash ^= ZOBRIST_PIECES[piece][i];

      if (*fen != 'K' && *fen != 'k')
        board->piecesCounts += PIECE_COUNT_IDX[piece];
    } else if (*fen >= '0' && *fen <= '9') {
      int offset = *fen - '1';
      i += offset;
    } else if (*fen == '/') {
      i--;
    }

    fen++;
  }

  fen++;

  board->side = (*fen++ == 'w' ? 0 : 1);
  board->xside = board->side ^ 1;
  board->mat = board->side == WHITE ? board->mat : -board->mat;

  fen++;

  while (*fen != ' ') {
    if (*fen == 'K') {
      board->castling |= 8;
    } else if (*fen == 'Q') {
      board->castling |= 4;
    } else if (*fen == 'k') {
      board->castling |= 2;
    } else if (*fen == 'q') {
      board->castling |= 1;
    }

    fen++;
  }

  fen++;

  if (*fen != '-') {
    int f = fen[0] - 'a';
    int r = 8 - (fen[1] - '0');

    board->epSquare = r * 8 + f;
  } else {
    board->epSquare = 0;
  }

  while (*fen && *fen != ' ')
    fen++;

  sscanf(fen, " %d", &board->halfMove);

  SetOccupancies(board);
  SetSpecialPieces(board);

  board->zobrist = Zobrist(board);
}

void BoardToFen(char* fen, Board* board) {
  for (int r = 0; r < 8; r++) {
    int c = 0;
    for (int f = 0; f < 8; f++) {
      int sq = 8 * r + f;
      int piece = board->squares[sq];

      if (piece != NO_PIECE) {
        if (c)
          *fen++ = c + '0'; // append the amount of empty sqs

        *fen++ = PIECE_TO_CHAR[piece];
        c = 0;
      } else {
        c++;
      }
    }

    if (c)
      *fen++ = c + '0';

    *fen++ = (r == 7) ? ' ' : '/';
  }

  *fen++ = board->side ? 'b' : 'w';
  *fen++ = ' ';

  if (board->castling) {
    if (board->castling & 0x8)
      *fen++ = 'K';
    if (board->castling & 0x4)
      *fen++ = 'Q';
    if (board->castling & 0x2)
      *fen++ = 'k';
    if (board->castling & 0x1)
      *fen++ = 'q';
  } else {
    *fen++ = '-';
  }

  *fen++ = ' ';

  sprintf(fen, "%s %d %d", board->epSquare ? SQ_TO_COORD[board->epSquare] : "-", board->halfMove, board->moveNo);
}

void PrintBoard(Board* board) {
  static char fenBuffer[128];

  for (int i = 0; i < 64; i++) {
    if (file(i) == 0)
      printf(" %d ", 8 - rank(i));

    if (board->squares[i] == NO_PIECE)
      printf(" .");
    else
      printf(" %c", PIECE_TO_CHAR[board->squares[i]]);

    if ((i & 7) == 7)
      printf("\n");
  }

  printf("\n    a b c d e f g h\n");

  BoardToFen(fenBuffer, board);
  printf(" %s\n", fenBuffer);
  printf("\n\n");
}

inline int HasNonPawn(Board* board) { return (board->piecesCounts & NON_PAWN_PIECE_MASK[board->side]) != 0; }

inline int IsOCB(Board* board) {
  BitBoard nonBishopMaterial = board->pieces[QUEEN_WHITE] | board->pieces[QUEEN_BLACK] | board->pieces[ROOK_WHITE] |
                               board->pieces[ROOK_BLACK] | board->pieces[KNIGHT_WHITE] | board->pieces[KNIGHT_BLACK];

  return !nonBishopMaterial && bits(board->pieces[BISHOP_WHITE]) == 1 && bits(board->pieces[BISHOP_BLACK]) == 1 &&
         bits((board->pieces[BISHOP_WHITE] | board->pieces[BISHOP_BLACK]) & DARK_SQS) == 1;
}

inline int IsMaterialDraw(Board* board) {
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
  default:
    return 0;
  }
}

// Check each type of piece movement from given square and see if an enemy piece of that
// movement type is there. If so, then it's attacked
inline int IsSquareAttacked(int sq, int attackColor, BitBoard occupancy, Board* board) {
  if (GetPawnAttacks(sq, attackColor ^ 1) & board->pieces[PAWN[attackColor]])
    return 1;
  if (GetKnightAttacks(sq) & board->pieces[KNIGHT[attackColor]])
    return 1;
  if (GetBishopAttacks(sq, occupancy) & (board->pieces[BISHOP[attackColor]] | board->pieces[QUEEN[attackColor]]))
    return 1;
  if (GetRookAttacks(sq, occupancy) & (board->pieces[ROOK[attackColor]] | board->pieces[QUEEN[attackColor]]))
    return 1;
  if (GetKingAttacks(sq) & board->pieces[KING[attackColor]])
    return 1;

  return 0;
}

// Reset general piece locations on the board
inline void SetOccupancies(Board* board) {
  board->occupancies[WHITE] = EMPTY;
  board->occupancies[BLACK] = EMPTY;
  board->occupancies[BOTH] = EMPTY;

  for (int i = PAWN_WHITE; i <= KING_BLACK; i++)
    board->occupancies[i & 1] |= board->pieces[i];
  board->occupancies[BOTH] = board->occupancies[WHITE] | board->occupancies[BLACK];
}

// Special pieces are those giving check, and those that are pinned
// these must be recalculated every move for faster move legality purposes
inline void SetSpecialPieces(Board* board) {
  int kingSq = lsb(board->pieces[KING[board->side]]);

  // Reset pinned pieces
  board->pinned = EMPTY;
  // checked can be initialized easily with non-blockable checks
  board->checkers = (GetKnightAttacks(kingSq) & board->pieces[KNIGHT[board->xside]]) |
                    (GetPawnAttacks(kingSq, board->side) & board->pieces[PAWN[board->xside]]);

  // for each side
  for (int kingColor = WHITE; kingColor <= BLACK; kingColor++) {
    int enemyColor = 1 - kingColor;
    kingSq = lsb(board->pieces[KING[kingColor]]);

    // get full rook/bishop rays from the king that intersect that piece type of the enemy
    BitBoard enemyPiece =
        ((board->pieces[BISHOP[enemyColor]] | board->pieces[QUEEN[enemyColor]]) & GetBishopAttacks(kingSq, 0)) |
        ((board->pieces[ROOK[enemyColor]] | board->pieces[QUEEN[enemyColor]]) & GetRookAttacks(kingSq, 0));

    while (enemyPiece) {
      int sq = lsb(enemyPiece);

      // is something in the way
      BitBoard blockers = GetInBetweenSquares(kingSq, sq) & board->occupancies[BOTH];

      if (!blockers)
        // no? then its check
        board->checkers |= (enemyPiece & -enemyPiece);
      else if (bits(blockers) == 1)
        // just 1? then its pinned
        board->pinned |= (blockers & board->occupancies[kingColor]);

      // TODO: Similar logic can be applied for discoveries, but apply for self pieces

      popLsb(enemyPiece);
    }
  }
}

void MakeMove(Move move, Board* board) {
  assert(move != NULL_MOVE);

  int start = MoveStart(move);
  int end = MoveEnd(move);
  int piece = MovePiece(move);
  int promoted = MovePromo(move);
  int capture = MoveCapture(move);
  int dub = MoveDoublePush(move);
  int ep = MoveEP(move);
  int castle = MoveCastle(move);
  int captured = board->squares[end];

  // store hard to recalculate values
  board->zobristHistory[board->moveNo] = board->zobrist;
  board->pawnHashHistory[board->moveNo] = board->pawnHash;
  board->castlingHistory[board->moveNo] = board->castling;
  board->epSquareHistory[board->moveNo] = board->epSquare;
  board->captureHistory[board->moveNo] = NO_PIECE; // this might get overwritten
  board->halfMoveHistory[board->moveNo] = board->halfMove;
  board->checkersHistory[board->moveNo] = board->checkers;
  board->pinnedHistory[board->moveNo] = board->pinned;

  popBit(board->pieces[piece], start);
  setBit(board->pieces[piece], end);

  board->squares[start] = NO_PIECE;
  board->squares[end] = piece;

  board->mat += PSQT[piece][end] - PSQT[piece][start];

  board->zobrist ^= ZOBRIST_PIECES[piece][start];
  board->zobrist ^= ZOBRIST_PIECES[piece][end];

  if (piece == PAWN[board->side]) {
    board->halfMove = 0; // reset on pawn move

    board->pawnHash ^= ZOBRIST_PIECES[piece][start];
    board->pawnHash ^= ZOBRIST_PIECES[piece][end];
  } else
    board->halfMove++;

  if (capture && !ep) {
    board->captureHistory[board->moveNo] = captured;
    popBit(board->pieces[captured], end);

    board->mat += PSQT[captured][end];

    board->zobrist ^= ZOBRIST_PIECES[captured][end];
    if (captured == PAWN[board->xside])
      board->pawnHash ^= ZOBRIST_PIECES[captured][end];

    board->piecesCounts -= PIECE_COUNT_IDX[captured]; // when there's a capture, we need to update our piece counts
    board->halfMove = 0;                              // reset on capture
  }

  if (promoted) {
    popBit(board->pieces[piece], end);
    setBit(board->pieces[promoted], end);

    board->squares[end] = promoted;

    board->mat += PSQT[promoted][end] - PSQT[piece][end];

    board->zobrist ^= ZOBRIST_PIECES[piece][end];
    board->zobrist ^= ZOBRIST_PIECES[promoted][end];
    board->pawnHash ^= ZOBRIST_PIECES[piece][end];

    board->piecesCounts -= PIECE_COUNT_IDX[piece];
    board->piecesCounts += PIECE_COUNT_IDX[promoted];
  }

  if (ep) {
    // ep has to be handled specially since the end location won't remove the opponents pawn
    popBit(board->pieces[PAWN[board->xside]], end - PAWN_DIRECTIONS[board->side]);

    board->squares[end - PAWN_DIRECTIONS[board->side]] = NO_PIECE;

    board->mat += PSQT[PAWN[board->xside]][end - PAWN_DIRECTIONS[board->side]];

    board->zobrist ^= ZOBRIST_PIECES[PAWN[board->xside]][end - PAWN_DIRECTIONS[board->side]];
    board->pawnHash ^= ZOBRIST_PIECES[PAWN[board->xside]][end - PAWN_DIRECTIONS[board->side]];

    board->piecesCounts -= PIECE_COUNT_IDX[PAWN[board->xside]];
    board->halfMove = 0; // this is a capture
  }

  if (board->epSquare) {
    board->zobrist ^= ZOBRIST_EP_KEYS[board->epSquare];
    board->epSquare = 0;
  }

  if (dub) {
    board->epSquare = end - PAWN_DIRECTIONS[board->side];
    board->zobrist ^= ZOBRIST_EP_KEYS[board->epSquare];
  }

  if (castle) {
    // move the rook during a castle, the king encoding is handled automatically
    if (end == G1) {
      popBit(board->pieces[ROOK[WHITE]], H1);
      setBit(board->pieces[ROOK[WHITE]], F1);

      board->squares[H1] = NO_PIECE;
      board->squares[F1] = ROOK[WHITE];

      board->mat += PSQT[ROOK_WHITE][F1] - PSQT[ROOK_WHITE][H1];

      board->zobrist ^= ZOBRIST_PIECES[ROOK[WHITE]][H1];
      board->zobrist ^= ZOBRIST_PIECES[ROOK[WHITE]][F1];
    } else if (end == C1) {
      popBit(board->pieces[ROOK[WHITE]], A1);
      setBit(board->pieces[ROOK[WHITE]], D1);

      board->squares[A1] = NO_PIECE;
      board->squares[D1] = ROOK[WHITE];

      board->mat += PSQT[ROOK_WHITE][D1] - PSQT[ROOK_WHITE][A1];

      board->zobrist ^= ZOBRIST_PIECES[ROOK[WHITE]][A1];
      board->zobrist ^= ZOBRIST_PIECES[ROOK[WHITE]][D1];
    } else if (end == G8) {
      popBit(board->pieces[ROOK[BLACK]], H8);
      setBit(board->pieces[ROOK[BLACK]], F8);

      board->mat += PSQT[ROOK_BLACK][F8] - PSQT[ROOK_BLACK][H8];

      board->squares[H8] = NO_PIECE;
      board->squares[F8] = ROOK[BLACK];

      board->zobrist ^= ZOBRIST_PIECES[ROOK[BLACK]][H8];
      board->zobrist ^= ZOBRIST_PIECES[ROOK[BLACK]][F8];
    } else if (end == C8) {
      popBit(board->pieces[ROOK[BLACK]], A8);
      setBit(board->pieces[ROOK[BLACK]], D8);

      board->squares[A8] = NO_PIECE;
      board->squares[D8] = ROOK[BLACK];

      board->mat += PSQT[ROOK_BLACK][D8] - PSQT[ROOK_BLACK][A8];

      board->zobrist ^= ZOBRIST_PIECES[ROOK[BLACK]][A8];
      board->zobrist ^= ZOBRIST_PIECES[ROOK[BLACK]][D8];
    }
  }

  board->zobrist ^= ZOBRIST_CASTLE_KEYS[board->castling];
  board->castling &= CASTLING_RIGHTS[start];
  board->castling &= CASTLING_RIGHTS[end];
  board->zobrist ^= ZOBRIST_CASTLE_KEYS[board->castling];

  SetOccupancies(board);

  board->moveNo++;
  board->xside = board->side;
  board->side ^= 1;
  board->zobrist ^= ZOBRIST_SIDE_KEY;
  board->mat = -board->mat;

  // special pieces must be loaded after the side has changed
  // this is because the new side to move will be the one in check
  SetSpecialPieces(board);

  // Prefetch the hash entry for this board position
  TTPrefetch(board->zobrist);
}

void UndoMove(Move move, Board* board) {
  int start = MoveStart(move);
  int end = MoveEnd(move);
  int piece = MovePiece(move);
  int promoted = MovePromo(move);
  int capture = MoveCapture(move);
  int ep = MoveEP(move);
  int castle = MoveCastle(move);

  board->side = board->xside;
  board->xside ^= 1;
  board->moveNo--;
  board->mat = -board->mat;

  // reload historical values
  board->epSquare = board->epSquareHistory[board->moveNo];
  board->castling = board->castlingHistory[board->moveNo];
  board->zobrist = board->zobristHistory[board->moveNo];
  board->pawnHash = board->pawnHashHistory[board->moveNo];
  board->halfMove = board->halfMoveHistory[board->moveNo];
  board->checkers = board->checkersHistory[board->moveNo];
  board->pinned = board->pinnedHistory[board->moveNo];

  popBit(board->pieces[piece], end);
  setBit(board->pieces[piece], start);

  board->squares[end] = NO_PIECE;
  board->squares[start] = piece;

  board->mat -= PSQT[piece][end] - PSQT[piece][start];

  if (capture) {
    int captured = board->captureHistory[board->moveNo];
    setBit(board->pieces[captured], end);

    if (!ep) {
      board->squares[end] = captured;

      board->mat -= PSQT[captured][end];

      board->piecesCounts += PIECE_COUNT_IDX[captured];
    }
  }

  if (promoted) {
    popBit(board->pieces[promoted], end);

    board->mat -= PSQT[promoted][end] - PSQT[piece][end];

    board->piecesCounts -= PIECE_COUNT_IDX[promoted];
    board->piecesCounts += PIECE_COUNT_IDX[piece];
  }

  if (ep) {
    setBit(board->pieces[PAWN[board->xside]], end - PAWN_DIRECTIONS[board->side]);
    board->squares[end - PAWN_DIRECTIONS[board->side]] = PAWN[board->xside];

    board->mat -= PSQT[PAWN[board->xside]][end - PAWN_DIRECTIONS[board->side]];

    board->piecesCounts += PIECE_COUNT_IDX[PAWN[board->xside]];
  }

  if (castle) {
    // put the rook back
    if (end == G1) {
      popBit(board->pieces[ROOK[WHITE]], F1);
      setBit(board->pieces[ROOK[WHITE]], H1);

      board->squares[F1] = NO_PIECE;
      board->squares[H1] = ROOK[WHITE];

      board->mat -= PSQT[ROOK_WHITE][F1] - PSQT[ROOK_WHITE][H1];
    } else if (end == C1) {
      popBit(board->pieces[ROOK[WHITE]], D1);
      setBit(board->pieces[ROOK[WHITE]], A1);

      board->squares[D1] = NO_PIECE;
      board->squares[A1] = ROOK[WHITE];

      board->mat -= PSQT[ROOK_WHITE][D1] - PSQT[ROOK_WHITE][A1];
    } else if (end == G8) {
      popBit(board->pieces[ROOK[BLACK]], F8);
      setBit(board->pieces[ROOK[BLACK]], H8);

      board->squares[F8] = NO_PIECE;
      board->squares[H8] = ROOK[BLACK];

      board->mat -= PSQT[ROOK_BLACK][F8] - PSQT[ROOK_BLACK][H8];
    } else if (end == C8) {
      popBit(board->pieces[ROOK[BLACK]], D8);
      setBit(board->pieces[ROOK[BLACK]], A8);

      board->squares[D8] = NO_PIECE;
      board->squares[A8] = ROOK[BLACK];

      board->mat -= PSQT[ROOK_BLACK][D8] - PSQT[ROOK_BLACK][A8];
    }
  }

  SetOccupancies(board);
}

// this is NOT a legality checker for ALL moves
// it is only used by move generation for king moves, castles, and ep captures
int IsMoveLegal(Move move, Board* board) {
  int start = MoveStart(move);
  int end = MoveEnd(move);

  if (MoveEP(move)) {
    // ep is checked by just applying the move
    int kingSq = lsb(board->pieces[KING[board->side]]);
    int captureSq = end - PAWN_DIRECTIONS[board->side];
    BitBoard newOccupancy = board->occupancies[BOTH];
    popBit(newOccupancy, start);
    popBit(newOccupancy, captureSq);
    setBit(newOccupancy, end);

    // EP can only be illegal due to crazy discover checks
    return !(GetBishopAttacks(kingSq, newOccupancy) &
             (board->pieces[BISHOP[board->xside]] | board->pieces[QUEEN[board->xside]])) &&
           !(GetRookAttacks(kingSq, newOccupancy) &
             (board->pieces[ROOK[board->xside]] | board->pieces[QUEEN[board->xside]]));
  } else if (MoveCastle(move)) {
    int step = (end > start) ? -1 : 1;

    // pieces in-between have been checked, now check that it's not castling through or into check
    for (int i = end; i != start; i += step) {
      if (IsSquareAttacked(i, board->xside, board->occupancies[BOTH], board))
        return 0;
    }

    return 1;
  } else if (MovePiece(move) >= KING[WHITE]) {
    BitBoard kingOff = board->occupancies[BOTH];
    popBit(kingOff, start);
    // check king attacks with it off board, because it may move along the checking line
    return !IsSquareAttacked(end, board->xside, kingOff, board);
  }

  return 1;
}

inline int IsRepetition(Board* board) {
  // Check as far back as the last non-reversible move
  for (int i = board->moveNo - 2; i >= 0 && i >= board->moveNo - board->halfMove; i -= 2) {
    if (board->zobristHistory[i] == board->zobrist)
      return 1;
  }

  return 0;
}

void MakeNullMove(Board* board) {
  board->zobristHistory[board->moveNo] = board->zobrist;
  board->castlingHistory[board->moveNo] = board->castling;
  board->epSquareHistory[board->moveNo] = board->epSquare;
  board->captureHistory[board->moveNo] = NO_PIECE;
  board->halfMoveHistory[board->moveNo] = board->halfMove;
  board->checkersHistory[board->moveNo] = board->checkers;
  board->pinnedHistory[board->moveNo] = board->pinned;

  board->halfMove++;

  if (board->epSquare)
    board->zobrist ^= ZOBRIST_EP_KEYS[board->epSquare];
  board->epSquare = 0;

  board->zobrist ^= ZOBRIST_SIDE_KEY;

  board->moveNo++;
  board->side = board->xside;
  board->xside ^= 1;
  board->mat = -board->mat;

  // Prefetch the hash entry for this board position
  TTPrefetch(board->zobrist);
}

void UndoNullMove(Board* board) {
  board->side = board->xside;
  board->xside ^= 1;
  board->moveNo--;
  board->mat = -board->mat;

  board->zobrist = board->zobristHistory[board->moveNo];
  board->castling = board->castlingHistory[board->moveNo];
  board->epSquare = board->epSquareHistory[board->moveNo];
  board->halfMove = board->halfMoveHistory[board->moveNo];
  board->checkers = board->checkersHistory[board->moveNo];
  board->pinned = board->pinnedHistory[board->moveNo];
}