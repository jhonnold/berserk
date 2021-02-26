#include <stdio.h>

#include "move.h"
#include "movegen.h"
#include "types.h"

const int CHAR_TO_PIECE[] = {['P'] = 0, ['N'] = 2, ['B'] = 4, ['R'] = 6, ['Q'] = 8, ['K'] = 10,
                              ['p'] = 1, ['n'] = 3, ['b'] = 5, ['r'] = 7, ['q'] = 9, ['k'] = 11};
const char* PIECE_TO_CHAR = "PpNnBbRrQqKk";
const char* PROMOTION_TO_CHAR = "ppnnbbrrqqkk";
const char* SQ_TO_COORD[] = {
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8", "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6", "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4", "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2", "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
};

Move parseMove(char* moveStr, Board* board) {
  MoveList moveList[1];
  generateMoves(moveList, board, 0);

  int start = (moveStr[0] - 'a') + (8 - (moveStr[1] - '0')) * 8;
  int end = (moveStr[2] - 'a') + (8 - (moveStr[3] - '0')) * 8;

  for (int i = 0; i < moveList->count; i++) {
    Move match = moveList->moves[i];
    if (start != moveStart(match) || end != moveEnd(match))
      continue;

    int promotedPiece = movePromo(match);
    if (!promotedPiece)
      return match;

    if (PROMOTION_TO_CHAR[promotedPiece] == moveStr[4])
      return match;
  }

  return 0;
}

char* moveStr(Move move) {
  static char buffer[6];

  if (movePromo(move)) {
    sprintf(buffer, "%s%s%c", SQ_TO_COORD[moveStart(move)], SQ_TO_COORD[moveEnd(move)], PROMOTION_TO_CHAR[movePromo(move)]);
  } else {
    sprintf(buffer, "%s%s", SQ_TO_COORD[moveStart(move)], SQ_TO_COORD[moveEnd(move)]);
  }

  return buffer;
}