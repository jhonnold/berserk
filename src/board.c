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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "move.h"
#include "movegen.h"
#include "nn.h"
#include "transposition.h"
#include "types.h"
#include "uci.h"
#include "zobrist.h"

const int PAWN[] = {PAWN_WHITE, PAWN_BLACK};
const int KNIGHT[] = {KNIGHT_WHITE, KNIGHT_BLACK};
const int BISHOP[] = {BISHOP_WHITE, BISHOP_BLACK};
const int ROOK[] = {ROOK_WHITE, ROOK_BLACK};
const int QUEEN[] = {QUEEN_WHITE, QUEEN_BLACK};
const int KING[] = {KING_WHITE, KING_BLACK};

// way to identify pieces are the same "type" (i.e. WHITE_PAWN and BLACK_PAWN)
const int PIECE_TYPE[13] = {PAWN_TYPE, PAWN_TYPE,  KNIGHT_TYPE, KNIGHT_TYPE, BISHOP_TYPE, BISHOP_TYPE, ROOK_TYPE,
                            ROOK_TYPE, QUEEN_TYPE, QUEEN_TYPE,  KING_TYPE,   KING_TYPE,   NO_PIECE / 2};

const int8_t PSQT[] = {0,  1,  2,  3,  3,  2,  1,  0,  4,  5,  6,  7,  7,  6,  5,  4,  8,  9,  10, 11, 11, 10,
                       9,  8,  12, 13, 14, 15, 15, 14, 13, 12, 16, 17, 18, 19, 19, 18, 17, 16, 20, 21, 22, 23,
                       23, 22, 21, 20, 24, 25, 26, 27, 27, 26, 25, 24, 28, 29, 30, 31, 31, 30, 29, 28};

// piece count key bit mask idx
const uint64_t PIECE_COUNT_IDX[] = {1ULL << 0,  1ULL << 4,  1ULL << 8,  1ULL << 12, 1ULL << 16,
                                    1ULL << 20, 1ULL << 24, 1ULL << 28, 1ULL << 32, 1ULL << 36};

const uint64_t NON_PAWN_PIECE_MASK[] = {0x0F0F0F0F00, 0xF0F0F0F000};

// reset the board to an empty state
void ClearBoard(Board* board) {
  memset(board->pieces, 0, sizeof(board->pieces));
  memset(board->occupancies, 0, sizeof(board->occupancies));
  memset(board->zobristHistory, 0, sizeof(board->zobristHistory));
  memset(board->castlingHistory, 0, sizeof(board->castlingHistory));
  memset(board->captureHistory, 0, sizeof(board->captureHistory));
  memset(board->halfMoveHistory, 0, sizeof(board->halfMoveHistory));

  for (int i = 0; i < 64; i++)
    board->squares[i] = NO_PIECE;

  board->piecesCounts = 0ULL;
  board->zobrist = 0ULL;

  board->side = WHITE;
  board->xside = BLACK;

  board->epSquare = 0;
  board->castling = 0;
  board->moveNo = 0;
  board->halfMove = 0;
  board->phase = 0;
}

void ParseFen(char* fen, Board* board) {
  ClearBoard(board);

  for (int i = 0; i < 64; i++) {
    if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z')) {
      int piece = CHAR_TO_PIECE[(int)*fen];
      setBit(board->pieces[piece], i);
      board->squares[i] = piece;

      board->phase += PHASE_VALUES[PIECE_TYPE[piece]];

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

  fen++;

  int whiteKing = lsb(board->pieces[KING_WHITE] & RANK_1);
  int blackKing = lsb(board->pieces[KING_BLACK] & RANK_8);

  int whiteKingFile = file(whiteKing);
  int blackKingFile = file(blackKing);

  while (*fen != ' ') {
    if (*fen == 'K') {
      board->castling |= 8;
      board->castleRooks[0] = msb(board->pieces[ROOK_WHITE] & RANK_1);
    } else if (*fen == 'Q') {
      board->castling |= 4;
      board->castleRooks[1] = lsb(board->pieces[ROOK_WHITE] & RANK_1);
    } else if (*fen >= 'A' && *fen <= 'H') {
      int file = *fen - 'A';
      board->castling |= (file > whiteKingFile ? 8 : 4);
      board->castleRooks[file > whiteKingFile ? 0 : 1] = A1 + file;
    } else if (*fen == 'k') {
      board->castling |= 2;
      board->castleRooks[2] = msb(board->pieces[ROOK_BLACK] & RANK_8);
    } else if (*fen == 'q') {
      board->castling |= 1;
      board->castleRooks[3] = lsb(board->pieces[ROOK_BLACK] & RANK_8);
    } else if (*fen >= 'a' && *fen <= 'h') {
      int file = *fen - 'a';
      board->castling |= (file > blackKingFile ? 2 : 1);
      board->castleRooks[file > blackKingFile ? 2 : 3] = A8 + file;
    }

    fen++;
  }

  for (int i = 0; i < 64; i++) {
    board->castlingRights[i] = board->castling;

    if (i == whiteKing)
      board->castlingRights[i] &= 3;
    else if (i == blackKing)
      board->castlingRights[i] &= 12;
    else if (i == board->castleRooks[0] && (board->castling & 8))
      board->castlingRights[i] ^= 8;
    else if (i == board->castleRooks[1] && (board->castling & 4))
      board->castlingRights[i] ^= 4;
    else if (i == board->castleRooks[2] && (board->castling & 2))
      board->castlingRights[i] ^= 2;
    else if (i == board->castleRooks[3] && (board->castling & 1))
      board->castlingRights[i] ^= 1;
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

  int halfMove;
  sscanf(fen, " %d", &halfMove);
  board->halfMove = halfMove;

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
      *fen++ = CHESS_960 ? 'A' + file(board->castleRooks[0]) : 'K';
    if (board->castling & 0x4)
      *fen++ = CHESS_960 ? 'A' + file(board->castleRooks[1]) : 'Q';
    if (board->castling & 0x2)
      *fen++ = CHESS_960 ? 'a' + file(board->castleRooks[2]) : 'k';
    if (board->castling & 0x1)
      *fen++ = CHESS_960 ? 'a' + file(board->castleRooks[3]) : 'q';
  } else {
    *fen++ = '-';
  }

  *fen++ = ' ';

  sprintf(fen, "%s %d %d", board->epSquare ? SQ_TO_COORD[board->epSquare] : "-", board->halfMove, board->moveNo);
}

void PrintBoard(Board* board) {
  static char fenBuffer[128];

  for (int r = 0; r < 8; r++) {
    printf("+-------+-------+-------+-------+-------+-------+-------+-------+\n");
    printf("|");
    for (int f = 0; f < 16; f++) {
      if (f == 8)
        printf("\n|");

      int sq = r * 8 + (f > 7 ? f - 8 : f);

      if (f < 8) {
        if (board->squares[sq] == NO_PIECE)
          printf("       |");
        else
          printf("   %c   |", PIECE_TO_CHAR[board->squares[sq]]);
      } else {
        printf("       |");
      }
    }
    printf("\n");
  }
  printf("+-------+-------+-------+-------+-------+-------+-------+-------+\n");

  BoardToFen(fenBuffer, board);
  printf("\nFEN: %s\n\n", fenBuffer);
}

inline int HasNonPawn(Board* board) {
  return bits(board->occupancies[board->side] ^ board->pieces[KING[board->side]] ^ board->pieces[PAWN[board->side]]);
}

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

// Reset general piece locations on the board
inline void SetOccupancies(Board* board) {
  board->occupancies[WHITE] = 0;
  board->occupancies[BLACK] = 0;
  board->occupancies[BOTH] = 0;

  for (int i = PAWN_WHITE; i <= KING_BLACK; i++)
    board->occupancies[i & 1] |= board->pieces[i];
  board->occupancies[BOTH] = board->occupancies[WHITE] | board->occupancies[BLACK];
}

// Special pieces are those giving check, and those that are pinned
// these must be recalculated every move for faster move legality purposes
inline void SetSpecialPieces(Board* board) {
  int kingSq = lsb(board->pieces[KING[board->side]]);

  // Reset pinned pieces
  board->pinned = 0;
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

void MakeMove(Move move, Board* board) { MakeMoveUpdate(move, board, 1); }

void MakeMoveUpdate(Move move, Board* board, int update) {
  NNUpdate wUpdates[1] = {0};
  NNUpdate bUpdates[1] = {0};

  int start = MoveStart(move);
  int end = MoveEnd(move);
  int piece = MovePiece(move);
  int promoted = MovePromo(move);
  int capture = MoveCapture(move);
  int dub = MoveDoublePush(move);
  int ep = MoveEP(move);
  int castle = MoveCastle(move);
  int captured = board->squares[end];

  int wkingSq = lsb(board->pieces[KING_WHITE]);
  int bkingSq = lsb(board->pieces[KING_BLACK]);

  // store hard to recalculate values
  board->zobristHistory[board->moveNo] = board->zobrist;
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

  board->zobrist ^= ZOBRIST_PIECES[piece][start];
  board->zobrist ^= ZOBRIST_PIECES[piece][end];

  AddRemoval(FeatureIdx(piece, start, wkingSq, WHITE), wUpdates);
  AddRemoval(FeatureIdx(piece, start, bkingSq, BLACK), bUpdates);
  AddAddition(FeatureIdx(promoted ? promoted : piece, end, wkingSq, WHITE), wUpdates);
  AddAddition(FeatureIdx(promoted ? promoted : piece, end, bkingSq, BLACK), bUpdates);

  if (piece == PAWN[board->side])
    board->halfMove = 0; // reset on pawn move
  else
    board->halfMove++;

  if (capture && !ep) {
    board->captureHistory[board->moveNo] = captured;
    popBit(board->pieces[captured], end);

    board->zobrist ^= ZOBRIST_PIECES[captured][end];

    AddRemoval(FeatureIdx(captured, end, wkingSq, WHITE), wUpdates);
    AddRemoval(FeatureIdx(captured, end, bkingSq, BLACK), bUpdates);

    board->piecesCounts -= PIECE_COUNT_IDX[captured]; // when there's a capture, we need to update our piece counts
    board->halfMove = 0;                              // reset on capture

    board->phase -= PHASE_VALUES[PIECE_TYPE[captured]];
  }

  if (promoted) {
    popBit(board->pieces[piece], end);
    setBit(board->pieces[promoted], end);

    board->squares[end] = promoted;

    board->zobrist ^= ZOBRIST_PIECES[piece][end];
    board->zobrist ^= ZOBRIST_PIECES[promoted][end];

    board->piecesCounts -= PIECE_COUNT_IDX[piece];
    board->piecesCounts += PIECE_COUNT_IDX[promoted];

    board->phase += PHASE_VALUES[PIECE_TYPE[promoted]];
  }

  if (ep) {
    // ep has to be handled specially since the end location won't remove the opponents pawn
    popBit(board->pieces[PAWN[board->xside]], end - PAWN_DIRECTIONS[board->side]);

    board->squares[end - PAWN_DIRECTIONS[board->side]] = NO_PIECE;

    board->zobrist ^= ZOBRIST_PIECES[PAWN[board->xside]][end - PAWN_DIRECTIONS[board->side]];

    AddRemoval(FeatureIdx(PAWN[board->xside], end - PAWN_DIRECTIONS[board->side], wkingSq, WHITE), wUpdates);
    AddRemoval(FeatureIdx(PAWN[board->xside], end - PAWN_DIRECTIONS[board->side], bkingSq, BLACK), bUpdates);

    board->piecesCounts -= PIECE_COUNT_IDX[PAWN[board->xside]];
    board->halfMove = 0; // this is a capture

    // skip the phase as pawns = 0
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
      popBit(board->pieces[ROOK[WHITE]], board->castleRooks[0]);
      setBit(board->pieces[ROOK[WHITE]], F1);

      if (board->castleRooks[0] != end)
        board->squares[board->castleRooks[0]] = NO_PIECE;
      board->squares[F1] = ROOK[WHITE];

      board->zobrist ^= ZOBRIST_PIECES[ROOK[WHITE]][board->castleRooks[0]];
      board->zobrist ^= ZOBRIST_PIECES[ROOK[WHITE]][F1];

      AddRemoval(FeatureIdx(ROOK_WHITE, board->castleRooks[0], wkingSq, WHITE), wUpdates);
      AddRemoval(FeatureIdx(ROOK_WHITE, board->castleRooks[0], bkingSq, BLACK), bUpdates);
      AddAddition(FeatureIdx(ROOK_WHITE, F1, wkingSq, WHITE), wUpdates);
      AddAddition(FeatureIdx(ROOK_WHITE, F1, bkingSq, BLACK), bUpdates);
    } else if (end == C1) {
      popBit(board->pieces[ROOK[WHITE]], board->castleRooks[1]);
      setBit(board->pieces[ROOK[WHITE]], D1);

      if (board->castleRooks[1] != end)
        board->squares[board->castleRooks[1]] = NO_PIECE;
      board->squares[D1] = ROOK[WHITE];

      board->zobrist ^= ZOBRIST_PIECES[ROOK[WHITE]][board->castleRooks[1]];
      board->zobrist ^= ZOBRIST_PIECES[ROOK[WHITE]][D1];

      AddRemoval(FeatureIdx(ROOK_WHITE, board->castleRooks[1], wkingSq, WHITE), wUpdates);
      AddRemoval(FeatureIdx(ROOK_WHITE, board->castleRooks[1], bkingSq, BLACK), bUpdates);
      AddAddition(FeatureIdx(ROOK_WHITE, D1, wkingSq, WHITE), wUpdates);
      AddAddition(FeatureIdx(ROOK_WHITE, D1, bkingSq, BLACK), bUpdates);
    } else if (end == G8) {
      popBit(board->pieces[ROOK[BLACK]], board->castleRooks[2]);
      setBit(board->pieces[ROOK[BLACK]], F8);

      if (board->castleRooks[2] != end)
        board->squares[board->castleRooks[2]] = NO_PIECE;
      board->squares[F8] = ROOK[BLACK];

      board->zobrist ^= ZOBRIST_PIECES[ROOK[BLACK]][board->castleRooks[2]];
      board->zobrist ^= ZOBRIST_PIECES[ROOK[BLACK]][F8];

      AddRemoval(FeatureIdx(ROOK_BLACK, board->castleRooks[2], wkingSq, WHITE), wUpdates);
      AddRemoval(FeatureIdx(ROOK_BLACK, board->castleRooks[2], bkingSq, BLACK), bUpdates);
      AddAddition(FeatureIdx(ROOK_BLACK, F8, wkingSq, WHITE), wUpdates);
      AddAddition(FeatureIdx(ROOK_BLACK, F8, bkingSq, BLACK), bUpdates);
    } else if (end == C8) {
      popBit(board->pieces[ROOK[BLACK]], board->castleRooks[3]);
      setBit(board->pieces[ROOK[BLACK]], D8);

      if (board->castleRooks[3] != end)
        board->squares[board->castleRooks[3]] = NO_PIECE;
      board->squares[D8] = ROOK[BLACK];

      board->zobrist ^= ZOBRIST_PIECES[ROOK[BLACK]][board->castleRooks[3]];
      board->zobrist ^= ZOBRIST_PIECES[ROOK[BLACK]][D8];

      AddRemoval(FeatureIdx(ROOK_BLACK, board->castleRooks[3], wkingSq, WHITE), wUpdates);
      AddRemoval(FeatureIdx(ROOK_BLACK, board->castleRooks[3], bkingSq, BLACK), bUpdates);
      AddAddition(FeatureIdx(ROOK_BLACK, D8, wkingSq, WHITE), wUpdates);
      AddAddition(FeatureIdx(ROOK_BLACK, D8, bkingSq, BLACK), bUpdates);
    }
  }

  board->zobrist ^= ZOBRIST_CASTLE_KEYS[board->castling];
  board->castling &= board->castlingRights[start];
  board->castling &= board->castlingRights[end];
  board->zobrist ^= ZOBRIST_CASTLE_KEYS[board->castling];

  SetOccupancies(board);

  board->moveNo++;
  board->ply++;
  board->xside = board->side;
  board->side ^= 1;
  board->zobrist ^= ZOBRIST_SIDE_KEY;

  // special pieces must be loaded after the side has changed
  // this is because the new side to move will be the one in check
  SetSpecialPieces(board);

  if (update) {
    if ((piece == KING_WHITE || piece == KING_BLACK) && (start & 4) != (end & 4)) {
      if (piece == KING_WHITE) {
        RefreshAccumulator(board->accumulators[WHITE][board->ply], board, WHITE);
        ApplyUpdates(board, BLACK, bUpdates);
      } else {
        ApplyUpdates(board, WHITE, wUpdates);
        RefreshAccumulator(board->accumulators[BLACK][board->ply], board, BLACK);
      }
    } else {
      ApplyUpdates(board, WHITE, wUpdates);
      ApplyUpdates(board, BLACK, bUpdates);
    }
  }

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
  board->ply--;

  // reload historical values
  board->epSquare = board->epSquareHistory[board->moveNo];
  board->castling = board->castlingHistory[board->moveNo];
  board->zobrist = board->zobristHistory[board->moveNo];
  board->halfMove = board->halfMoveHistory[board->moveNo];
  board->checkers = board->checkersHistory[board->moveNo];
  board->pinned = board->pinnedHistory[board->moveNo];

  popBit(board->pieces[piece], end);
  setBit(board->pieces[piece], start);

  board->squares[end] = NO_PIECE;
  board->squares[start] = piece;

  if (capture) {
    int captured = board->captureHistory[board->moveNo];
    setBit(board->pieces[captured], end);

    if (!ep) {
      board->squares[end] = captured;

      board->piecesCounts += PIECE_COUNT_IDX[captured];

      board->phase += PHASE_VALUES[PIECE_TYPE[captured]];
    }
  }

  if (promoted) {
    popBit(board->pieces[promoted], end);

    board->piecesCounts -= PIECE_COUNT_IDX[promoted];
    board->piecesCounts += PIECE_COUNT_IDX[piece];

    board->phase -= PHASE_VALUES[PIECE_TYPE[promoted]];
  }

  if (ep) {
    setBit(board->pieces[PAWN[board->xside]], end - PAWN_DIRECTIONS[board->side]);
    board->squares[end - PAWN_DIRECTIONS[board->side]] = PAWN[board->xside];

    board->piecesCounts += PIECE_COUNT_IDX[PAWN[board->xside]];
  }

  if (castle) {
    // put the rook back
    if (end == G1) {
      popBit(board->pieces[ROOK[WHITE]], F1);
      setBit(board->pieces[ROOK[WHITE]], board->castleRooks[0]);

      if (start != F1)
        board->squares[F1] = NO_PIECE;

      board->squares[board->castleRooks[0]] = ROOK[WHITE];
    } else if (end == C1) {
      popBit(board->pieces[ROOK[WHITE]], D1);
      setBit(board->pieces[ROOK[WHITE]], board->castleRooks[1]);

      if (start != D1)
        board->squares[D1] = NO_PIECE;
      board->squares[board->castleRooks[1]] = ROOK[WHITE];
    } else if (end == G8) {
      popBit(board->pieces[ROOK[BLACK]], F8);
      setBit(board->pieces[ROOK[BLACK]], board->castleRooks[2]);

      if (start != F8)
        board->squares[F8] = NO_PIECE;
      board->squares[board->castleRooks[2]] = ROOK[BLACK];
    } else if (end == C8) {
      popBit(board->pieces[ROOK[BLACK]], D8);
      setBit(board->pieces[ROOK[BLACK]], board->castleRooks[3]);

      if (start != D8)
        board->squares[D8] = NO_PIECE;
      board->squares[board->castleRooks[3]] = ROOK[BLACK];
    }
  }

  SetOccupancies(board);
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

  // Prefetch the hash entry for this board position
  TTPrefetch(board->zobrist);
}

void UndoNullMove(Board* board) {
  board->side = board->xside;
  board->xside ^= 1;
  board->moveNo--;

  board->zobrist = board->zobristHistory[board->moveNo];
  board->castling = board->castlingHistory[board->moveNo];
  board->epSquare = board->epSquareHistory[board->moveNo];
  board->halfMove = board->halfMoveHistory[board->moveNo];
  board->checkers = board->checkersHistory[board->moveNo];
  board->pinned = board->pinnedHistory[board->moveNo];
}

inline int IsRepetition(Board* board, int ply) {
  int reps = 0;

  // Check as far back as the last non-reversible move
  for (int i = board->moveNo - 2; i >= 0 && i >= board->moveNo - board->halfMove; i -= 2) {
    if (board->zobristHistory[i] == board->zobrist) {
      if (i > board->moveNo - ply) // within our search tree
        return 1;

      reps++;
      if (reps == 2) // 3-fold before+including root
        return 1;
    }
  }

  return 0;
}

int MoveIsLegal(Move move, Board* board) {
  int piece = MovePiece(move);
  int start = MoveStart(move);
  int end = MoveEnd(move);

  // moving piece isn't where it says it is
  if (!move || piece > KING_BLACK || (piece & 1) != board->side || piece != board->squares[start])
    return 0;

  // can't capture our own piece unless castling
  if (board->squares[end] != NO_PIECE && !MoveCastle(move) &&
      (!MoveCapture(move) || (board->squares[end] & 1) == board->side))
    return 0;

  // can't capture air
  if (MoveCapture(move) && board->squares[end] == NO_PIECE && !MoveEP(move))
    return 0;

  // non-pawns can't promote/dp/ep
  if ((MovePromo(move) || MoveDoublePush(move) || MoveEP(move)) && PIECE_TYPE[piece] != PAWN_TYPE)
    return 0;

  // non-kings can't castle
  if (MoveCastle(move) && (PIECE_TYPE[piece] != KING_TYPE || MoveCapture(move)))
    return 0;

  BitBoard possible = -1ULL;
  if (getBit(board->pinned, start))
    possible &= GetPinnedMovementSquares(start, lsb(board->pieces[KING[board->side]]));

  if (board->checkers)
    possible &= GetInBetweenSquares(lsb(board->checkers), lsb(board->pieces[KING[board->side]])) | board->checkers;

  if (bits(board->checkers) > 1 && PIECE_TYPE[piece] != KING_TYPE)
    return 0;

  switch (PIECE_TYPE[piece]) {
  case KING_TYPE:
    if (!MoveCastle(move) && !getBit(GetKingAttacks(start), end))
      return 0;
    break;
  case KNIGHT_TYPE:
    return !!getBit(GetKnightAttacks(start) & possible, end);
  case BISHOP_TYPE:
    return !!getBit(GetBishopAttacks(start, board->occupancies[BOTH]) & possible, end);
  case ROOK_TYPE:
    return !!getBit(GetRookAttacks(start, board->occupancies[BOTH]) & possible, end);
  case QUEEN_TYPE:
    return !!getBit(GetQueenAttacks(start, board->occupancies[BOTH]) & possible, end);
  case PAWN_TYPE:
    if (MoveEP(move))
      break;

    BitBoard atx = GetPawnAttacks(start, board->side);
    BitBoard adv = board->side == WHITE ? ShiftN(bit(start)) : ShiftS(bit(start));
    adv &= ~board->occupancies[BOTH];

    if (MovePromo(move))
      return !!getBit((board->side == WHITE ? RANK_8 : RANK_1) & ((atx & board->occupancies[board->xside]) | adv) &
                          possible,
                      end) &&
             MovePromo(move) >= KNIGHT_WHITE && MovePromo(move) <= QUEEN_BLACK &&
             (MovePromo(move) & 1) == board->side && !MoveDoublePush(move);

    adv |= board->side == WHITE ? ShiftN(adv & RANK_3) : ShiftS(adv & RANK_6);
    adv &= ~board->occupancies[BOTH];

    return !!getBit((board->side == WHITE ? ~RANK_8 : ~RANK_1) & ((atx & board->occupancies[board->xside]) | adv) &
                        possible,
                    end) &&
           MoveDoublePush(move) == (abs(start - end) == 16);
  }

  if (MoveEP(move)) {
    if (!MoveCapture(move) || MoveDoublePush(move) || MovePromo(move) || !board->epSquare || board->epSquare != end ||
        PIECE_TYPE[piece] != PAWN_TYPE || !getBit(GetPawnAttacks(start, board->side), board->epSquare))
      return 0;
  }

  if (MoveCastle(move)) {
    if (board->checkers)
      return 0;

    int kingSq = lsb(board->pieces[KING[board->side]]);

    if (start != kingSq)
      return 0;

    if ((board->side == WHITE && end != G1 && end != C1) || (board->side == BLACK && end != G8 && end != C8))
      return 0;

    if (end == G1) {
      if (!(board->castling & 0x8))
        return 0;

      if (getBit(board->pinned, board->castleRooks[0]))
        return 0;

      BitBoard between =
          GetInBetweenSquares(kingSq, G1) | GetInBetweenSquares(board->castleRooks[0], F1) | bit(G1) | bit(F1);
      if ((board->occupancies[BOTH] ^ board->pieces[KING_WHITE] ^ bit(board->castleRooks[0])) & between)
        return 0;
    }

    if (end == C1) {
      if (!(board->castling & 0x4))
        return 0;

      if (getBit(board->pinned, board->castleRooks[1]))
        return 0;

      BitBoard between =
          GetInBetweenSquares(kingSq, C1) | GetInBetweenSquares(board->castleRooks[1], D1) | bit(C1) | bit(D1);
      if ((board->occupancies[BOTH] ^ board->pieces[KING_WHITE] ^ bit(board->castleRooks[1])) & between)
        return 0;
    }

    if (end == G8) {
      if (!(board->castling & 0x2))
        return 0;

      if (getBit(board->pinned, board->castleRooks[2]))
        return 0;

      BitBoard between =
          GetInBetweenSquares(kingSq, G8) | GetInBetweenSquares(board->castleRooks[2], F8) | bit(G8) | bit(F8);
      if ((board->occupancies[BOTH] ^ board->pieces[KING_BLACK] ^ bit(board->castleRooks[2])) & between)
        return 0;
    }

    if (end == C8) {
      if (!(board->castling & 0x1))
        return 0;

      if (getBit(board->pinned, board->castleRooks[3]))
        return 0;

      BitBoard between =
          GetInBetweenSquares(kingSq, C8) | GetInBetweenSquares(board->castleRooks[3], D8) | bit(C8) | bit(D8);
      if ((board->occupancies[BOTH] ^ board->pieces[KING_BLACK] ^ bit(board->castleRooks[3])) & between)
        return 0;
    }
  }

  // this is a legality checker for ep/king/castles (used by movegen)
  return IsMoveLegal(move, board);
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
      if (AttacksToSquare(board, i, board->occupancies[BOTH]) & board->occupancies[board->xside])
        return 0;
    }

    return 1;
  } else if (MovePiece(move) >= KING[WHITE]) {
    return !(AttacksToSquare(board, end, board->occupancies[BOTH] ^ board->pieces[KING[board->side]]) &
             board->occupancies[board->xside]);
  }

  return 1;
}
