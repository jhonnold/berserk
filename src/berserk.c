#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "search.h"
#include "transposition.h"
#include "types.h"
#include "uci.h"
#include "util.h"
#include "zobrist.h"

char* benchmarks[5] = {
    "1qr3k1/p2nbppp/bp2p3/3p4/3P4/1P2PNP1/P2Q1PBP/1N2R1K1 b - -",
    "1r2r1k1/3bnppp/p2q4/2RPp3/4P3/6P1/2Q1NPBP/2R3K1 w - -",
    "2b1k2r/2p2ppp/1qp4n/7B/1p2P3/5Q2/PPPr2PP/R2N1R1K b k -",
    "2b5/1p4k1/p2R2P1/4Np2/1P3Pp1/1r6/5K2/8 w - -",
    "2brr1k1/ppq2ppp/2pb1n2/8/3NP3/2P2P2/P1Q2BPP/1R1R1BK1 w - -",
};

int main(int argc, char** argv) {
#ifndef WIN32
  initSignalHandlers();
#endif
  initPSQT();
  initAttacks();
  initPawnSpans();
  initZobristKeys();

  ttInit(32);

  Board board[1];
  memset(board, 0, sizeof(Board));
  parseFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", board);

  // This is to be compliant for OpenBench
  if (argc > 1 && !strncmp(argv[1], "bench", 5)) {
    long totalNodes = 0;
    long startTime = getTimeMs();

    SearchParams params[1];
    params->depth = 15;
    params->timeset = 0;
    params->stopped = 0;
    params->quit = 0;
    params->endTime = 0;

    for (int i = 0; i < 5; i++) {
      parseFen(benchmarks[i], board);

      SearchData data[1];
      Search(board, params, data);

      totalNodes += data->nodes;
    }

    long endTime = getTimeMs();
    long time = endTime - startTime;
    printf("\n\nBench Results: %ld nodes %d nps\n\n", totalNodes, (int)(1000.0 * totalNodes / (time + 1)));
  } else {
    UCI(board);
  }

  ttFree();
  return 0;
}
