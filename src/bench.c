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
#include "nn/evaluate.h"
#include "search.h"
#include "thread.h"
#include "transposition.h"
#include "types.h"
#include "uci.h"
#include "util.h"

uint64_t activations[N_HIDDEN][N_HIDDEN] = {0};

void Bench(int depth) {
  Limits.depth   = depth;
  Limits.multiPV = 1;
  Limits.hitrate = INT_MAX;
  Limits.max     = INT_MAX;
  Limits.timeset = 0;

  FILE* fin = fopen("/home/jhonnold/fens", "r");
  char * line = NULL;
  size_t len = 0;
  ssize_t read;

  int total = 0;
  while (total < 10000000 && (read = getline(&line, &len, fin)) != -1) {
    total++;

    ParseFen(line, &Threads.threads[0]->board);
    Predict(&Threads.threads[0]->board);    

    if (total % 1000 == 0) printf("%d\n", total);

    // TTClear();
    // SearchClear();

    // Limits.start = GetTimeMS();
    // StartSearch(&board, 0);
    // ThreadWaitUntilSleep(Threads.threads[0]);
  }

  // for (size_t i = 0; i < N_HIDDEN; i++)
  //   printf(" %lu", activations[i]);
  // printf("\n");

  SortAndWrite(activations);

  // for (size_t i = 0; i < N_HIDDEN; i++)
  //   printf(" %lu", activations[i]);
  // printf("\n");
}