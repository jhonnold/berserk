#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "types.h"
#include "uci.h"
#include "zobrist.h"

int main() {
  initPositionValues();
  initAttacks();
  initPawnSpans();
  initZobristKeys();

  Board board[1];
  memset(board, 0, sizeof(Board));

  parseFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", board);
  UCI(board);

  return 0;
}
