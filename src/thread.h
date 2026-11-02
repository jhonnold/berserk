// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2024 Jay Honnold

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

#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>
#include <stdatomic.h>

#include "types.h"

typedef struct {
  Move move;
  int seldepth;
  int score, previousScore, avgScore;
  uint64_t nodes;
  PV pv;
} RootMove;

enum {
  THREAD_SLEEP,
  THREAD_SEARCH,
  THREAD_TT_CLEAR,
  THREAD_SEARCH_CLEAR,
  THREAD_EXIT,
  THREAD_RESUME
};

struct ThreadData {
  int idx, multiPV, depth, completedDepth, seldepth;
  atomic_uint_fast64_t nodes, tbhits;

  int nmpMinPly, npmColor;

  Accumulator* accumulators;
  AccumulatorKingState* refreshTable;

  Board board;

  int contempt[2];
  int previousScore;
  int numRootMoves;
  RootMove rootMoves[MAX_MOVES];

  Move counters[12][64];         // counter move butterfly table
  int16_t hh[2][2][2][64 * 64];  // history heuristic butterfly table (stm / threatened)
  int16_t ch[2][12][64][12][64]; // continuation move history table
  int16_t caph[12][64][2][7];    // capture history (piece - to - defeneded - captured_type)

  int16_t contCorrection[12][64][12][64];

  int action, calls;
  pthread_t nativeThread;
  pthread_mutex_t mutex;
  pthread_cond_t sleep;
};

typedef struct {
  ThreadData* threads[2048];
  int count;

  pthread_mutex_t mutex, lock;
  pthread_cond_t sleep;

  uint8_t init, searching, sleeping, stopOnPonderHit;
  atomic_uchar ponder, stop;
} ThreadPool;

extern ThreadPool Threads;

void ThreadWaitUntilSleep(ThreadData* thread);
void ThreadWait(ThreadData* thread, atomic_uchar* cond);
void ThreadWake(ThreadData* thread, int action);
void ThreadsSetNumber(int n);
void ThreadsExit();
void ThreadsInit();

void SetupMainThread(Board* board);
void SetupOtherThreads(Board* board);

uint64_t NodesSearched();
uint64_t TBHits();

#endif