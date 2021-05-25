// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2021 Jay Honnold

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

#include <stdlib.h>
#include <string.h>

#include "types.h"

// initialize a pool of threads
ThreadData* CreatePool(int count) {
  ThreadData* threads = malloc(count * sizeof(ThreadData));

  for (int i = 0; i < count; i++) {
    // allow reference to one another
    threads[i].idx = i;
    threads[i].threads = threads;
    threads[i].count = count;
  }

  return threads;
}

// initialize a pool prepping to start a search
void InitPool(Board* board, SearchParams* params, ThreadData* threads) {
  for (int i = 0; i < threads->count; i++) {
    threads[i].params = params;

    threads[i].data.nodes = 0;
    threads[i].data.seldepth = 0;
    threads[i].data.ply = 0;
    threads[i].data.tbhits = 0;

    // empty unneeded data
    memset(&threads[i].data.skipMove, 0, sizeof(threads[i].data.skipMove));
    memset(&threads[i].data.evals, 0, sizeof(threads[i].data.evals));
    memset(&threads[i].data.moves, 0, sizeof(threads[i].data.moves));

    // need full copies of the board
    memcpy(&threads[i].board, board, sizeof(Board));
  }
}

void ResetThreadPool(Board* board, SearchParams* params, ThreadData* threads) {
  for (int i = 0; i < threads->count; i++) {
    threads[i].params = params;

    threads[i].data.nodes = 0;
    threads[i].data.seldepth = 0;
    threads[i].data.ply = 0;
    threads[i].data.tbhits = 0;

    // empty ALL data
    memset(&threads[i].data.skipMove, 0, sizeof(threads[i].data.skipMove));
    memset(&threads[i].data.evals, 0, sizeof(threads[i].data.evals));
    memset(&threads[i].data.moves, 0, sizeof(threads[i].data.moves));
    memset(&threads[i].data.killers, 0, sizeof(threads[i].data.killers));
    memset(&threads[i].data.counters, 0, sizeof(threads[i].data.counters));
    memset(&threads[i].data.hh, 0, sizeof(threads[i].data.hh));
    memset(&threads[i].pawnHashTable, 0, PAWN_TABLE_SIZE * sizeof(PawnHashEntry));

    // need full copies of the board
    memcpy(&threads[i].board, board, sizeof(Board));
  }
}

// sum node counts
uint64_t NodesSearched(ThreadData* threads) {
  uint64_t nodes = 0;
  for (int i = 0; i < threads->count; i++)
    nodes += threads[i].data.nodes;

  return nodes;
}

uint64_t TBHits(ThreadData* threads) {
  uint64_t tbhits = 0;
  for (int i = 0; i < threads->count; i++)
    tbhits += threads[i].data.tbhits;

  return tbhits;
}