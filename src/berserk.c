#include <assert.h>
#include <stdio.h>
#include <sys/time.h>

#include "attacks.h"
#include "board.h"
#include "eval.h"
#include "types.h"
#include "uci.h"
#include "zobrist.h"

int main() {
  initPositionValues();
  initAttacks();
  initZobristKeys();

  Board board[1];
  parseFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", board);
  UCI(board);

  return 0;
}
