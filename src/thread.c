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

#include "thread.h"

#include <stdlib.h>
#include <string.h>

#include "eval.h"
#include "search.h"
#include "types.h"
#include "util.h"

void* AlignedMalloc(int size) {
  void* mem = malloc(size + ALIGN_ON + sizeof(void*));
  void** ptr = (void**)((uintptr_t)(mem + ALIGN_ON + sizeof(void*)) & ~(ALIGN_ON - 1));
  ptr[-1] = mem;
  return ptr;
}

void AlignedFree(void* ptr) { free(((void**)ptr)[-1]); }

// initialize a pool of threads
ThreadData* CreatePool(int count) {
  ThreadData* threads = malloc(count * sizeof(ThreadData));

  for (int i = 0; i < count; i++) {
    // allow reference to one another
    threads[i].idx = i;
    threads[i].threads = threads;
    threads[i].count = count;
    threads[i].accumulators[WHITE] = (Accumulator*)AlignedMalloc(sizeof(Accumulator) * (MAX_SEARCH_PLY + 1));
    threads[i].accumulators[BLACK] = (Accumulator*)AlignedMalloc(sizeof(Accumulator) * (MAX_SEARCH_PLY + 1));
  }

  return threads;
}

// initialize a pool prepping to start a search
void InitPool(Board* board, SearchParams* params, ThreadData* threads, SearchResults* results) {
  for (int i = 0; i < threads->count; i++) {
    threads[i].params = params;
    threads[i].results = results;

    threads[i].data.nodes = 0;
    threads[i].data.seldepth = 0;
    threads[i].data.ply = 0;
    threads[i].data.tbhits = 0;

    // empty unneeded data
    memset(&threads[i].data.skipMove, 0, sizeof(threads[i].data.skipMove));
    memset(&threads[i].data.evals, 0, sizeof(threads[i].data.evals));
    memset(&threads[i].data.tm, 0, sizeof(threads[i].data.tm));

    // set the moves arr as an offset of 2
    memset(&threads[i].data.searchMoves, 0, sizeof(threads[i].data.searchMoves));
    threads[i].data.moves = &threads[i].data.searchMoves[2];

    memset(&threads[i].scores, 0, sizeof(threads[i].scores));
    memset(&threads[i].bestMoves, 0, sizeof(threads[i].bestMoves));
    memset(&threads[i].pvs, 0, sizeof(threads[i].pvs));

    // need full copies of the board
    memcpy(&threads[i].board, board, sizeof(Board));
    threads[i].board.accumulators[WHITE] = threads[i].accumulators[WHITE];
    threads[i].board.accumulators[BLACK] = threads[i].accumulators[BLACK];
  }
}

void ResetThreadPool(ThreadData* threads) {
  for (int i = 0; i < threads->count; i++) {
    threads[i].results = NULL;

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
    memset(&threads[i].data.ch, 0, sizeof(threads[i].data.ch));
    memset(&threads[i].data.fh, 0, sizeof(threads[i].data.fh));
    memset(&threads[i].data.th, 0, sizeof(threads[i].data.th));
  }
}

void FreeThreads(ThreadData* threads) {
  for (int i = 0; i < threads->count; i++) {
    AlignedFree(threads[i].accumulators[WHITE]);
    AlignedFree(threads[i].accumulators[BLACK]);
  }

  free(threads);
}

// sum node counts
uint64_t NodesSearched(ThreadData* threads) {
  uint64_t nodes = 0;
  for (int i = 0; i < threads->count; i++) nodes += threads[i].data.nodes;

  return nodes;
}

uint64_t TBHits(ThreadData* threads) {
  uint64_t tbhits = 0;
  for (int i = 0; i < threads->count; i++) tbhits += threads[i].data.tbhits;

  return tbhits;
}

int Seldepth(ThreadData* threads) {
  int seldepth = threads[0].data.seldepth;
  for (int i = 1; i < threads->count; i++) seldepth = max(seldepth, threads[i].data.seldepth);

  return seldepth;
}
