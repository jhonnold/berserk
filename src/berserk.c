#include <stdio.h>
#include <string.h>

#include "attacks.h"
#include "bench.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "random.h"
#include "search.h"

#include "transposition.h"
#include "types.h"
#include "uci.h"
#include "util.h"
#include "zobrist.h"

#ifdef TUNE
#include "texel.h"
#include "texelfilter.h"
#endif

int main(int argc, char** argv) {
  seedRandom(0);

#ifndef WIN32
  initSignalHandlers();
#endif
  initPSQT();
  initAttacks();
  initPawnSpans();
  initZobristKeys();
  initLMR();

  ttInit(32);

  Board board[1];
  memset(board, 0, sizeof(Board));
  parseFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", board);

  // This is to be compliant for OpenBench
  if (argc > 1 && !strncmp(argv[1], "bench", 5)) {
    Bench();
  }

// TODO: Texel code could use some cleanup
#ifdef TUNE
  else if (argc > 1 && !strncmp(argv[1], "tune", 4)) {
    Texel();
  } else if (argc > 1 && !strncmp(argv[1], "filter", 4)) {
    RunFilter();
  }
#endif
  else {
    UCI(board);
  }

  ttFree();
  return 0;
}
