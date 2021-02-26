#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "move.h"
#include "movegen.h"
#include "types.h"
#include "zobrist.h"

const BitBoard EMPTY = 0ULL;

const int PAWN[] = {0, 1};
const int KNIGHT[] = {2, 3};
const int BISHOP[] = {4, 5};
const int ROOK[] = {6, 7};
const int QUEEN[] = {8, 9};
const int KING[] = {10, 11};

// clang-format off
const int castlingRights[] = {
  14, 15, 15, 15, 12, 15, 15, 13, 
  15, 15, 15, 15, 15, 15, 15, 15, 
  15, 15, 15, 15, 15, 15, 15, 15, 
  15, 15, 15, 15, 15, 15, 15, 15, 
  15, 15, 15, 15, 15, 15, 15, 15, 
  15, 15, 15, 15, 15, 15, 15, 15, 
  15, 15, 15, 15, 15, 15, 15, 15, 
  11, 15, 15, 15, 3,  15, 15,  7
};

const int mirror[] = {
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

void clear(Board* board) {
  memset(board->pieces, EMPTY, sizeof(board->pieces));
  memset(board->occupancies, EMPTY, sizeof(board->occupancies));
  memset(board->gameMoves, EMPTY, sizeof(board->gameMoves));
  memset(board->zobristHistory, EMPTY, sizeof(board->zobristHistory));
  memset(board->castlingHistory, EMPTY, sizeof(board->castlingHistory));
  memset(board->captureHistory, EMPTY, sizeof(board->captureHistory));
  memset(board->killers, EMPTY, sizeof(board->killers));
  memset(board->counters, EMPTY, sizeof(board->counters));

  board->side = WHITE;
  board->xside = BLACK;

  board->epSquare = 0;
  board->castling = 0;
  board->moveNo = 0;
}

uint64_t zobrist(Board* board) {
  uint64_t hash = 0;

  for (int piece = 0; piece < 12; piece++) {
    BitBoard pieces = board->pieces[piece];

    while (pieces) {
      int sq = lsb(pieces);
      hash ^= ZOBRIST_PIECES[piece][sq];
      popLsb(pieces);
    }
  }

  if (board->epSquare)
    hash ^= ZOBRIST_EP_KEYS[board->epSquare];

  hash ^= ZOBRIST_CASTLE_KEYS[board->castling];

  if (board->side)
    hash ^= ZOBRIST_SIDE_KEY;

  return hash;
}

void parseFen(char* fen, Board* board) {
  clear(board);

  for (int i = 0; i < 64; i++) {
    if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z')) {
      int piece = CHAR_TO_PIECE[(int)*fen];
      setBit(board->pieces[piece], i);
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

  setOccupancies(board);
  setSpecialPieces(board);

  board->zobrist = zobrist(board);
}

void printBoard(Board* board) {
  for (int i = 0; i < 64; i++) {
    if (file(i) == 0)
      printf(" %d ", 8 - rank(i));

    if (getBit(board->occupancies[BOTH], i)) {
      for (int p = 0; p < 12; p++) {
        if (getBit(board->pieces[p], i)) {
          printf(" %c", PIECE_TO_CHAR[p]);
          break;
        }
      }
    } else {
      printf(" .");
    }

    if ((i & 7) == 7)
      printf("\n");
  }

  printf("\n    a b c d e f g h\n\n");
}

inline int isSquareAttacked(int sq, int attackColor, BitBoard occupancy, Board* board) {
  if (getPawnAttacks(sq, attackColor ^ 1) & board->pieces[PAWN[attackColor]])
    return 1;
  if (getKnightAttacks(sq) & board->pieces[KNIGHT[attackColor]])
    return 1;
  if (getBishopAttacks(sq, occupancy) & (board->pieces[BISHOP[attackColor]] | board->pieces[QUEEN[attackColor]]))
    return 1;
  if (getRookAttacks(sq, occupancy) & (board->pieces[ROOK[attackColor]] | board->pieces[QUEEN[attackColor]]))
    return 1;
  if (getKingAttacks(sq) & board->pieces[KING[attackColor]])
    return 1;

  return 0;
}

inline int inCheck(Board* board) { return board->checkers ? 1 : 0; }

inline int capturedPiece(Move move, Board* board) {
  for (int i = PAWN[board->xside]; i <= KING[board->xside]; i += 2)
    if (getBit(board->pieces[i], moveEnd(move)))
      return i;

  return -1;
}

inline void setOccupancies(Board* board) {
  memset(board->occupancies, EMPTY, sizeof(board->occupancies));

  for (int i = PAWN[WHITE]; i <= KING[BLACK]; i++)
    board->occupancies[i & 1] |= board->pieces[i];
  board->occupancies[BOTH] = board->occupancies[WHITE] | board->occupancies[BLACK];
}

inline void setSpecialPieces(Board* board) {
  int kingSq = lsb(board->pieces[KING[board->side]]);

  board->pinners = EMPTY;
  board->checkers = (getKnightAttacks(kingSq) & board->pieces[KNIGHT[board->xside]]) |
                    (getPawnAttacks(kingSq, board->side) & board->pieces[PAWN[board->xside]]);

  for (int kingColor = WHITE; kingColor <= BLACK; kingColor++) {
    int enemyColor = 1 - kingColor;
    kingSq = lsb(board->pieces[KING[kingColor]]);

    BitBoard enemyPiece =
        ((board->pieces[BISHOP[enemyColor]] | board->pieces[QUEEN[enemyColor]]) & getBishopAttacks(kingSq, 0)) |
        ((board->pieces[ROOK[enemyColor]] | board->pieces[QUEEN[enemyColor]]) & getRookAttacks(kingSq, 0));

    while (enemyPiece) {
      int sq = lsb(enemyPiece);

      BitBoard blockers = getInBetween(kingSq, sq) & board->occupancies[BOTH];

      if (!blockers)
        board->checkers |= (enemyPiece & -enemyPiece);
      else if (bits(blockers) == 1)
        board->pinners |= (blockers & board->occupancies[kingColor]);

      popLsb(enemyPiece);
    }
  }
}

void makeMove(Move move, Board* board) {
  int start = moveStart(move);
  int end = moveEnd(move);
  int piece = movePiece(move);
  int promoted = movePromo(move);
  int capture = moveCapture(move);
  int dub = moveDouble(move);
  int ep = moveEP(move);
  int castle = moveCastle(move);

  board->zobristHistory[board->moveNo] = board->zobrist;
  board->castlingHistory[board->moveNo] = board->castling;
  board->epSquareHistory[board->moveNo] = board->epSquare;
  board->gameMoves[board->moveNo] = move;
  board->captureHistory[board->moveNo] = -1;

  popBit(board->pieces[piece], start);
  setBit(board->pieces[piece], end);

  board->zobrist ^= ZOBRIST_PIECES[piece][start];
  board->zobrist ^= ZOBRIST_PIECES[piece][end];

  if (capture && !ep) {
    int captured = capturedPiece(move, board);
    board->captureHistory[board->moveNo] = captured;
    popBit(board->pieces[captured], end);
    board->zobrist ^= ZOBRIST_PIECES[captured][end];
  }

  if (promoted) {
    popBit(board->pieces[piece], end);
    setBit(board->pieces[promoted], end);

    board->zobrist ^= ZOBRIST_PIECES[piece][end];
    board->zobrist ^= ZOBRIST_PIECES[promoted][end];
  }

  if (ep) {
    popBit(board->pieces[PAWN[board->xside]], end - PAWN_DIRECTIONS[board->side]);
    board->zobrist ^= ZOBRIST_PIECES[PAWN[board->xside]][end - PAWN_DIRECTIONS[board->side]];
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
    if (end == G1) {
      popBit(board->pieces[ROOK[WHITE]], H1);
      setBit(board->pieces[ROOK[WHITE]], F1);

      board->zobrist ^= ZOBRIST_PIECES[ROOK[WHITE]][H1];
      board->zobrist ^= ZOBRIST_PIECES[ROOK[WHITE]][F1];
    } else if (end == C1) {
      popBit(board->pieces[ROOK[WHITE]], A1);
      setBit(board->pieces[ROOK[WHITE]], D1);

      board->zobrist ^= ZOBRIST_PIECES[ROOK[WHITE]][A1];
      board->zobrist ^= ZOBRIST_PIECES[ROOK[WHITE]][D1];
    } else if (end == G8) {
      popBit(board->pieces[ROOK[BLACK]], H8);
      setBit(board->pieces[ROOK[BLACK]], F8);

      board->zobrist ^= ZOBRIST_PIECES[ROOK[BLACK]][H8];
      board->zobrist ^= ZOBRIST_PIECES[ROOK[BLACK]][F8];
    } else if (end == C8) {
      popBit(board->pieces[ROOK[BLACK]], A8);
      setBit(board->pieces[ROOK[BLACK]], D8);

      board->zobrist ^= ZOBRIST_PIECES[ROOK[BLACK]][A8];
      board->zobrist ^= ZOBRIST_PIECES[ROOK[BLACK]][D8];
    }
  }

  board->zobrist ^= ZOBRIST_CASTLE_KEYS[board->castling];
  board->castling &= castlingRights[start];
  board->castling &= castlingRights[end];
  board->zobrist ^= ZOBRIST_CASTLE_KEYS[board->castling];

  setOccupancies(board);

  board->moveNo++;
  board->xside = board->side;
  board->side ^= 1;
  board->zobrist ^= ZOBRIST_SIDE_KEY;

  setSpecialPieces(board);
}

void undoMove(Move move, Board* board) {
  int start = moveStart(move);
  int end = moveEnd(move);
  int piece = movePiece(move);
  int promoted = movePromo(move);
  int capture = moveCapture(move);
  int ep = moveEP(move);
  int castle = moveCastle(move);

  board->side = board->xside;
  board->xside ^= 1;
  board->moveNo--;

  board->epSquare = board->epSquareHistory[board->moveNo];
  board->castling = board->castlingHistory[board->moveNo];
  board->zobrist = board->zobristHistory[board->moveNo];

  popBit(board->pieces[piece], end);
  setBit(board->pieces[piece], start);

  if (capture)
    setBit(board->pieces[board->captureHistory[board->moveNo]], end);

  if (promoted)
    popBit(board->pieces[promoted], end);

  if (ep)
    setBit(board->pieces[PAWN[board->xside]], end - PAWN_DIRECTIONS[board->side]);

  if (castle) {
    if (end == G1) {
      popBit(board->pieces[6], F1);
      setBit(board->pieces[6], H1);
    } else if (end == C1) {
      popBit(board->pieces[6], D1);
      setBit(board->pieces[6], A1);
    } else if (end == G8) {
      popBit(board->pieces[7], F8);
      setBit(board->pieces[7], H8);
    } else if (end == C8) {
      popBit(board->pieces[7], D8);
      setBit(board->pieces[7], A8);
    }
  }

  setOccupancies(board);
  setSpecialPieces(board);
}

int isLegal(Move move, Board* board) {
  int start = moveStart(move);
  int end = moveEnd(move);

  if (moveEP(move)) {
    int kingSq = lsb(board->pieces[KING[board->side]]);
    int captureSq = end - PAWN_DIRECTIONS[board->side];
    BitBoard newOccupancy = board->occupancies[BOTH];
    popBit(newOccupancy, start);
    popBit(newOccupancy, captureSq);
    setBit(newOccupancy, end);

    // EP can only be illegal due to crazy discover checks
    return !(getBishopAttacks(kingSq, newOccupancy) &
             (board->pieces[BISHOP[board->xside]] | board->pieces[QUEEN[board->xside]])) &&
           !(getRookAttacks(kingSq, newOccupancy) &
             (board->pieces[ROOK[board->xside]] | board->pieces[QUEEN[board->xside]]));
  } else if (moveCastle(move)) {
    int step = (end > start) ? -1 : 1;

    for (int i = end; i != start; i += step) {
      if (isSquareAttacked(i, board->xside, board->occupancies[BOTH], board))
        return 0;
    }

    return 1;
  } else if (movePiece(move) >= KING[WHITE]) {
    BitBoard kingOff = board->occupancies[BOTH];
    popBit(kingOff, start);
    // check king attacks with it off board, because it may move along the checking line
    return !isSquareAttacked(end, board->xside, kingOff, board);
  }

  return 1;
}

inline int isRepetition(Board* board) {
  for (int i = 0; i < board->moveNo; i++) {
    if (board->zobristHistory[i] == board->zobrist)
      return 1;
  }

  return 0;
}

void nullMove(Board* board) {
  board->zobristHistory[board->moveNo] = board->zobrist;
  board->castlingHistory[board->moveNo] = board->castling;
  board->epSquareHistory[board->moveNo] = board->epSquare;
  board->captureHistory[board->moveNo] = -1;

  if (board->epSquare)
    board->zobrist ^= ZOBRIST_EP_KEYS[board->epSquare];
  board->epSquare = 0;

  board->zobrist ^= ZOBRIST_SIDE_KEY;

  board->moveNo++;
  board->side = board->xside;
  board->xside ^= 1;
}

void undoNullMove(Board* board) {
  board->side = board->xside;
  board->xside ^= 1;
  board->moveNo--;

  board->zobrist = board->zobristHistory[board->moveNo];
  board->castling = board->castlingHistory[board->moveNo];
  board->epSquare = board->epSquareHistory[board->moveNo];
}