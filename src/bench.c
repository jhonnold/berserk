// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2023 Jay Honnold

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

#include "bench.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "move.h"
#include "search.h"
#include "thread.h"
#include "transposition.h"
#include "types.h"
#include "uci.h"
#include "util.h"

// Ethereal's bench set
const int NUM_BENCH_POSITIONS = 50;
char* benchmarks[]            = {
#include "files/bench.csv"
};

void Bench(int depth) {
  Board board;

  Limits.depth   = depth;
  Limits.multiPV = 1;
  Limits.hitrate = INT_MAX;
  Limits.max     = INT_MAX;
  Limits.timeset = 0;

  Move bestMoves[NUM_BENCH_POSITIONS];
  int scores[NUM_BENCH_POSITIONS];
  uint64_t nodes[NUM_BENCH_POSITIONS];
  long times[NUM_BENCH_POSITIONS];

  long startTime = GetTimeMS();
  for (int i = 0; i < NUM_BENCH_POSITIONS; i++) {
    ParseFen(benchmarks[i], &board);

    TTClear();
    SearchClear();

    Limits.start = GetTimeMS();
    StartSearch(&board, 0);
    ThreadWaitUntilSleep(Threads.threads[0]);
    times[i] = GetTimeMS() - Limits.start;

    bestMoves[i] = Threads.threads[0]->rootMoves[0].move;
    scores[i]    = Threads.threads[0]->rootMoves[0].score;
    nodes[i]     = Threads.threads[0]->nodes;
  }
  long totalTime = GetTimeMS() - startTime;

  printf("\n\n");
  for (int i = 0; i < NUM_BENCH_POSITIONS; i++) {
    printf("Bench [#%2d]: bestmove %5s score %5d %12" PRIu64 " nodes %8d nps | %71s\n",
           i + 1,
           MoveToStr(bestMoves[i], &board),
           (int) Normalize(scores[i]),
           nodes[i],
           (int) (1000.0 * nodes[i] / (times[i] + 1)),
           benchmarks[i]);
  }

  uint64_t totalNodes = 0;
  for (int i = 0; i < NUM_BENCH_POSITIONS; i++)
    totalNodes += nodes[i];

  printf("\nResults: %43" PRIu64 " nodes %8d nps\n\n", totalNodes, (int) (1000.0 * totalNodes / (totalTime + 1)));
}