// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2022 Jay Honnold

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
#include "util.h"

// Ethereal's bench set
const int NUM_BENCH_POSITIONS = 50;
char* benchmarks[] = {
#include "files/bench.csv"
};

void Bench() {
  Board board;
  SearchParams params = {.depth = 13, .multiPV = 1, .hitrate = 1000, .max = INT_MAX};
  ThreadData* threads = CreatePool(1);

  Move bestMoves[NUM_BENCH_POSITIONS];
  int scores[NUM_BENCH_POSITIONS];
  int nodes[NUM_BENCH_POSITIONS];
  long times[NUM_BENCH_POSITIONS];

  long startTime = GetTimeMS();
  for (int i = 0; i < NUM_BENCH_POSITIONS; i++) {
    ParseFen(benchmarks[i], &board);

    SearchResults results = {0};
    TTClear();
    ResetThreadPool(threads);
    InitPool(&board, &params, threads, &results);

    params.start = GetTimeMS();
    BestMove(&board, &params, threads, &results);
    times[i] = GetTimeMS() - params.start;

    bestMoves[i] = results.bestMoves[results.depth];
    scores[i] = results.scores[results.depth];
    nodes[i] = threads[0].data.nodes;
  }
  long totalTime = GetTimeMS() - startTime;

  printf("\n\n");
  for (int i = 0; i < NUM_BENCH_POSITIONS; i++) {
    printf("Bench [#%2d]: bestmove %5s score %5d %12d nodes %8d nps | %71s\n", i + 1, MoveToStr(bestMoves[i], &board),
           scores[i], nodes[i], (int)(1000.0 * nodes[i] / (times[i] + 1)), benchmarks[i]);
  }

  int totalNodes = 0;
  for (int i = 0; i < NUM_BENCH_POSITIONS; i++) totalNodes += nodes[i];

  printf("\nResults: %43d nodes %8d nps\n\n", totalNodes, (int)(1000.0 * totalNodes / (totalTime + 1)));

  free(threads);
}