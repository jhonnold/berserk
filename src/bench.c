#include <stdio.h>
#include <string.h>

#include "bench.h"
#include "board.h"
#include "search.h"
#include "types.h"
#include "util.h"

char* benchmarks[] = {"1qr3k1/p2nbppp/bp2p3/3p4/3P4/1P2PNP1/P2Q1PBP/1N2R1K1 b - -",
                      "1r2r1k1/3bnppp/p2q4/2RPp3/4P3/6P1/2Q1NPBP/2R3K1 w - -",
                      "2b1k2r/2p2ppp/1qp4n/7B/1p2P3/5Q2/PPPr2PP/R2N1R1K b k -",
                      "2b5/1p4k1/p2R2P1/4Np2/1P3Pp1/1r6/5K2/8 w - -",
                      "2brr1k1/ppq2ppp/2pb1n2/8/3NP3/2P2P2/P1Q2BPP/1R1R1BK1 w - -",
                      "2kr2nr/1pp3pp/p1pb4/4p2b/4P1P1/5N1P/PPPN1P2/R1B1R1K1 b - -",
                      "2r1k2r/1p1qbppp/p3pn2/3pBb2/3P4/1QN1P3/PP2BPPP/2R2RK1 b k -",
                      "2r1r1k1/pbpp1npp/1p1b3q/3P4/4RN1P/1P4P1/PB1Q1PB1/2R3K1 w - -",
                      "2r2k2/r4p2/2b1p1p1/1p1p2Pp/3R1P1P/P1P5/1PB5/2K1R3 w - -",
                      "2r3k1/5pp1/1p2p1np/p1q5/P1P4P/1P1Q1NP1/5PK1/R7 w - -",
                      "2r3qk/p5p1/1n3p1p/4PQ2/8/3B4/5P1P/3R2K1 w - -",
                      "3b4/3k1pp1/p1pP2q1/1p2B2p/1P2P1P1/P2Q3P/4K3/8 w - -",
                      "3n1r1k/2p1p1bp/Rn4p1/6N1/3P3P/2N1B3/2r2PP1/5RK1 w - -",
                      "3q1rk1/3rbppp/ppbppn2/1N6/2P1P3/BP6/P1B1QPPP/R3R1K1 w - -",
                      "3r1rk1/p1q4p/1pP1ppp1/2n1b1B1/2P5/6P1/P1Q2PBP/1R3RK1 w - -"};

void Bench() {
  long totalNodes = 0;
  long startTime = getTimeMs();

  Board board[1];
  memset(board, 0, sizeof(Board));

  SearchParams params[1];
  params->depth = 15;
  params->timeset = 0;
  params->stopped = 0;
  params->quit = 0;
  params->endTime = 0;

  for (int i = 0; i < 15; i++) {
    parseFen(benchmarks[i], board);

    SearchData data[1];
    Search(board, params, data);

    totalNodes += data->nodes;
  }

  long endTime = getTimeMs();
  long time = endTime - startTime;
  printf("\n\nBench Results: %ld nodes %d nps\n\n", totalNodes, (int)(1000.0 * totalNodes / (time + 1)));
}