#include <assert.h>
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

const int PAWN[] = {PAWN_WHITE, PAWN_BLACK};
const int KNIGHT[] = {KNIGHT_WHITE, KNIGHT_BLACK};
const int BISHOP[] = {BISHOP_WHITE, BISHOP_BLACK};
const int ROOK[] = {ROOK_WHITE, ROOK_BLACK};
const int QUEEN[] = {QUEEN_WHITE, QUEEN_BLACK};
const int KING[] = {KING_WHITE, KING_BLACK};

// way to identify if PAWN[WHITE] == PAWN[BLACK]
const int PIECE_TYPE[13] = {
    PAWN_TYPE, PAWN_TYPE, KNIGHT_TYPE, KNIGHT_TYPE, BISHOP_TYPE, BISHOP_TYPE,
    ROOK_TYPE, ROOK_TYPE, QUEEN_TYPE,  QUEEN_TYPE,  KING_TYPE,   KING_TYPE,
    6 // No piece
};

// clang-format off
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

const uint64_t PIECE_COUNT_IDX[] = {
    // P           p            N           n           B
    1ULL << 0, 1ULL << 4, 1ULL << 8, 1ULL << 12, 1ULL << 16,
    // b           R            r           Q           q
    1ULL << 20, 1ULL << 24, 1ULL << 28, 1ULL << 32, 1ULL << 36};

const uint64_t MAJOR_PIECE_COUNT_MASK[] = {0x0F0F0F0F00, 0xF0F0F0F000};

void clear(Board* board) {
  memset(board->pieces, EMPTY, sizeof(board->pieces));
  memset(board->occupancies, EMPTY, sizeof(board->occupancies));
  memset(board->zobristHistory, EMPTY, sizeof(board->zobristHistory));
  memset(board->castlingHistory, EMPTY, sizeof(board->castlingHistory));
  memset(board->captureHistory, EMPTY, sizeof(board->captureHistory));
  memset(board->halfMoveHistory, 0, sizeof(board->halfMoveHistory));

  for (int i = 0; i < 64; i++)
    board->squares[i] = NO_PIECE;

  board->piecesCounts = 0ULL;

  board->side = WHITE;
  board->xside = BLACK;

  board->epSquare = 0;
  board->castling = 0;
  board->moveNo = 0;
  board->halfMove = 0;
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
      board->squares[i] = piece;

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

  while (*fen != ' ')
    fen++;

  if (*fen != 'c') {
    fen++;
    sscanf(fen, "%d", &board->halfMove);
  } else {
    board->halfMove = 0;
  }

  setOccupancies(board);
  setSpecialPieces(board);

  board->zobrist = zobrist(board);
}

void toFen(char* fen, Board* board) {
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

void printBoard(Board* board) {
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

  printf("\n");

  char fen[256];
  toFen(fen, board);
  printf(" %s\n\n", fen);
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

inline int hasNonPawn(Board* board) { return (board->piecesCounts & MAJOR_PIECE_COUNT_MASK[board->side]) != 0; }

inline void setOccupancies(Board* board) {
  memset(board->occupancies, EMPTY, sizeof(board->occupancies));

  for (int i = PAWN_WHITE; i <= KING_BLACK; i++)
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
  assert(move != NULL_MOVE);

  int start = moveStart(move);
  int end = moveEnd(move);
  int piece = movePiece(move);
  int promoted = movePromo(move);
  int capture = moveCapture(move);
  int dub = moveDouble(move);
  int ep = moveEP(move);
  int castle = moveCastle(move);
  int captured = board->squares[end];

  board->zobristHistory[board->moveNo] = board->zobrist;
  board->castlingHistory[board->moveNo] = board->castling;
  board->epSquareHistory[board->moveNo] = board->epSquare;
  board->captureHistory[board->moveNo] = NO_PIECE;
  board->halfMoveHistory[board->moveNo] = board->halfMove;

  popBit(board->pieces[piece], start);
  setBit(board->pieces[piece], end);

  board->squares[start] = NO_PIECE;
  board->squares[end] = piece;

  board->zobrist ^= ZOBRIST_PIECES[piece][start];
  board->zobrist ^= ZOBRIST_PIECES[piece][end];

  if (piece == PAWN[board->side])
    board->halfMove = 0;
  else
    board->halfMove++;

  if (capture && !ep) {
    board->captureHistory[board->moveNo] = captured;
    popBit(board->pieces[captured], end);
    board->zobrist ^= ZOBRIST_PIECES[captured][end];
    board->piecesCounts -= PIECE_COUNT_IDX[captured];
    board->halfMove = 0;
  }

  if (promoted) {
    popBit(board->pieces[piece], end);
    setBit(board->pieces[promoted], end);

    board->squares[end] = promoted;

    board->zobrist ^= ZOBRIST_PIECES[piece][end];
    board->zobrist ^= ZOBRIST_PIECES[promoted][end];

    board->piecesCounts -= PIECE_COUNT_IDX[piece];
    board->piecesCounts += PIECE_COUNT_IDX[promoted];
  }

  if (ep) {
    popBit(board->pieces[PAWN[board->xside]], end - PAWN_DIRECTIONS[board->side]);
    board->squares[end - PAWN_DIRECTIONS[board->side]] = NO_PIECE;
    board->zobrist ^= ZOBRIST_PIECES[PAWN[board->xside]][end - PAWN_DIRECTIONS[board->side]];
    board->piecesCounts -= PIECE_COUNT_IDX[PAWN[board->xside]];
    board->halfMove = 0;
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

      board->squares[H1] = NO_PIECE;
      board->squares[F1] = ROOK[WHITE];

      board->zobrist ^= ZOBRIST_PIECES[ROOK[WHITE]][H1];
      board->zobrist ^= ZOBRIST_PIECES[ROOK[WHITE]][F1];
    } else if (end == C1) {
      popBit(board->pieces[ROOK[WHITE]], A1);
      setBit(board->pieces[ROOK[WHITE]], D1);

      board->squares[A1] = NO_PIECE;
      board->squares[D1] = ROOK[WHITE];

      board->zobrist ^= ZOBRIST_PIECES[ROOK[WHITE]][A1];
      board->zobrist ^= ZOBRIST_PIECES[ROOK[WHITE]][D1];
    } else if (end == G8) {
      popBit(board->pieces[ROOK[BLACK]], H8);
      setBit(board->pieces[ROOK[BLACK]], F8);

      board->squares[H8] = NO_PIECE;
      board->squares[F8] = ROOK[BLACK];

      board->zobrist ^= ZOBRIST_PIECES[ROOK[BLACK]][H8];
      board->zobrist ^= ZOBRIST_PIECES[ROOK[BLACK]][F8];
    } else if (end == C8) {
      popBit(board->pieces[ROOK[BLACK]], A8);
      setBit(board->pieces[ROOK[BLACK]], D8);

      board->squares[A8] = NO_PIECE;
      board->squares[D8] = ROOK[BLACK];

      board->zobrist ^= ZOBRIST_PIECES[ROOK[BLACK]][A8];
      board->zobrist ^= ZOBRIST_PIECES[ROOK[BLACK]][D8];
    }
  }

  board->zobrist ^= ZOBRIST_CASTLE_KEYS[board->castling];
  board->castling &= CASTLING_RIGHTS[start];
  board->castling &= CASTLING_RIGHTS[end];
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
  board->halfMove = board->halfMoveHistory[board->moveNo];

  popBit(board->pieces[piece], end);
  setBit(board->pieces[piece], start);

  board->squares[end] = NO_PIECE;
  board->squares[start] = piece;

  if (capture) {
    setBit(board->pieces[board->captureHistory[board->moveNo]], end);

    if (!ep) {
      board->squares[end] = board->captureHistory[board->moveNo];
      board->piecesCounts += PIECE_COUNT_IDX[board->captureHistory[board->moveNo]];
    }
  }

  if (promoted) {
    popBit(board->pieces[promoted], end);
    board->piecesCounts -= PIECE_COUNT_IDX[promoted];
    board->piecesCounts += PIECE_COUNT_IDX[piece];
  }

  if (ep) {
    setBit(board->pieces[PAWN[board->xside]], end - PAWN_DIRECTIONS[board->side]);
    board->squares[end - PAWN_DIRECTIONS[board->side]] = PAWN[board->xside];
    board->piecesCounts += PIECE_COUNT_IDX[PAWN[board->xside]];
  }

  if (castle) {
    if (end == G1) {
      popBit(board->pieces[ROOK[WHITE]], F1);
      setBit(board->pieces[ROOK[WHITE]], H1);

      board->squares[F1] = NO_PIECE;
      board->squares[H1] = ROOK[WHITE];
    } else if (end == C1) {
      popBit(board->pieces[ROOK[WHITE]], D1);
      setBit(board->pieces[ROOK[WHITE]], A1);

      board->squares[D1] = NO_PIECE;
      board->squares[A1] = ROOK[WHITE];
    } else if (end == G8) {
      popBit(board->pieces[ROOK[BLACK]], F8);
      setBit(board->pieces[ROOK[BLACK]], H8);

      board->squares[F8] = NO_PIECE;
      board->squares[H8] = ROOK[BLACK];
    } else if (end == C8) {
      popBit(board->pieces[ROOK[BLACK]], D8);
      setBit(board->pieces[ROOK[BLACK]], A8);

      board->squares[D8] = NO_PIECE;
      board->squares[A8] = ROOK[BLACK];
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
  for (int i = board->moveNo - 2; i >= 0 && i >= board->moveNo - board->halfMove; i -= 2) {
    if (board->zobristHistory[i] == board->zobrist)
      return 1;
  }

  return 0;
}

void nullMove(Board* board) {
  board->zobristHistory[board->moveNo] = board->zobrist;
  board->castlingHistory[board->moveNo] = board->castling;
  board->epSquareHistory[board->moveNo] = board->epSquare;
  board->captureHistory[board->moveNo] = NO_PIECE;
  board->halfMoveHistory[board->moveNo] = board->halfMove;

  board->halfMove++;

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
  board->halfMove = board->halfMoveHistory[board->moveNo];
}