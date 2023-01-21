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

#include "thread.h"

#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eval.h"
#include "nn.h"
#include "search.h"
#include "transposition.h"
#include "types.h"
#include "uci.h"
#include "util.h"

// Below is an implementation of CFish's Sleeping Threads
// This is implemented entirely with pThreads
ThreadPool Threads;

// Block until requested thread is sleeping
void ThreadWaitUntilSleep(ThreadData* thread) {
  pthread_mutex_lock(&thread->mutex);

  while (thread->action != THREAD_SLEEP)
    pthread_cond_wait(&thread->sleep, &thread->mutex);

  pthread_mutex_unlock(&thread->mutex);

  if (thread->idx == 0)
    Threads.searching = 0;
}

// Block thread until on condition
void ThreadWait(ThreadData* thread, atomic_uchar* cond) {
  pthread_mutex_lock(&thread->mutex);

  while (!atomic_load(cond))
    pthread_cond_wait(&thread->sleep, &thread->mutex);

  pthread_mutex_unlock(&thread->mutex);
}

// Wake a thread up with an action
void ThreadWake(ThreadData* thread, int action) {
  pthread_mutex_lock(&thread->mutex);

  if (action != THREAD_RESUME)
    thread->action = action;

  pthread_cond_signal(&thread->sleep);
  pthread_mutex_unlock(&thread->mutex);
}

// Idle loop that wakes into an action
void ThreadIdle(ThreadData* thread) {
  while (1) {
    pthread_mutex_lock(&thread->mutex);

    while (thread->action == THREAD_SLEEP) {
      pthread_cond_signal(&thread->sleep);
      pthread_cond_wait(&thread->sleep, &thread->mutex);
    }

    pthread_mutex_unlock(&thread->mutex);

    if (thread->action == THREAD_EXIT)
      break;
    else if (thread->action == THREAD_TT_CLEAR) {
      TTClearPart(thread->idx);
    } else if (thread->action == THREAD_SEARCH_CLEAR) {
      SearchClearThread(thread);
    } else {
      if (thread->idx)
        Search(thread);
      else
        MainSearch();
    }

    thread->action = THREAD_SLEEP;
  }
}

// Build a thread
void* ThreadInit(void* arg) {
  int i = (intptr_t) arg;

  ThreadData* thread = calloc(1, sizeof(ThreadData));
  thread->idx        = i;

  // Alloc all the necessary accumulators
  thread->accumulators = (Accumulator*) AlignedMalloc(sizeof(Accumulator) * (MAX_SEARCH_PLY + 1));
  thread->refreshTable = (AccumulatorKingState*) AlignedMalloc(sizeof(AccumulatorKingState) * 2 * 2 * N_KING_BUCKETS);
  ResetRefreshTable(thread->refreshTable);

  // Copy these onto the board for easier access within the engine
  thread->board.accumulators = thread->accumulators;
  thread->board.refreshTable = thread->refreshTable;

  pthread_mutex_init(&thread->mutex, NULL);
  pthread_cond_init(&thread->sleep, NULL);

  Threads.threads[i] = thread;

  pthread_mutex_lock(&Threads.mutex);
  Threads.init = 0;
  pthread_cond_signal(&Threads.sleep);
  pthread_mutex_unlock(&Threads.mutex);

  ThreadIdle(thread);

  return NULL;
}

// Create a thread with idx i
void ThreadCreate(int i) {
  pthread_t thread;

  Threads.init = 1;
  pthread_mutex_lock(&Threads.mutex);
  pthread_create(&thread, NULL, ThreadInit, (void*) (intptr_t) i);

  while (Threads.init)
    pthread_cond_wait(&Threads.sleep, &Threads.mutex);
  pthread_mutex_unlock(&Threads.mutex);

  Threads.threads[i]->nativeThread = thread;
}

// Teardown and free a thread
void ThreadDestroy(ThreadData* thread) {
  pthread_mutex_lock(&thread->mutex);
  thread->action = THREAD_EXIT;
  pthread_cond_signal(&thread->sleep);
  pthread_mutex_unlock(&thread->mutex);

  pthread_join(thread->nativeThread, NULL);
  pthread_cond_destroy(&thread->sleep);
  pthread_mutex_destroy(&thread->mutex);

  AlignedFree(thread->accumulators);
  AlignedFree(thread->refreshTable);

  free(thread);
}

// Build the pool to a certain amnt
void ThreadsSetNumber(int n) {
  while (Threads.count < n)
    ThreadCreate(Threads.count++);
  while (Threads.count > n)
    ThreadDestroy(Threads.threads[--Threads.count]);

  if (n == 0)
    Threads.searching = 0;
}

// End
void ThreadsExit() {
  ThreadsSetNumber(0);

  pthread_cond_destroy(&Threads.sleep);
  pthread_mutex_destroy(&Threads.mutex);
}

// Start
void ThreadsInit() {
  pthread_mutex_init(&Threads.mutex, NULL);
  pthread_cond_init(&Threads.sleep, NULL);

  Threads.count = 1;
  ThreadCreate(0);
}

INLINE void InitRootMove(RootMove* rm, Move move) {
  rm->move = move;

  rm->previousScore = rm->score = -CHECKMATE;

  rm->pv.moves[0] = move;
  rm->pv.count = 1;

  rm->nodes = 0;
}

void SetupMainThread(Board* board) {
  ThreadData* mainThread = Threads.threads[0];
  mainThread->calls      = 0;
  mainThread->nodes      = 0;
  mainThread->tbhits     = 0;
  mainThread->seldepth   = 1;

  memcpy(&mainThread->board, board, offsetof(Board, accumulators));

  if (Limits.searchMoves) {
    for (int i = 0; i < Limits.searchable.count; i++)
      InitRootMove(&mainThread->rootMoves[i], Limits.searchable.moves[i]);

    mainThread->numRootMoves = Limits.searchable.count;
  } else {
    SimpleMoveList ml[1];
    RootMoves(ml, board);

    for (int i = 0; i < ml->count; i++)
      InitRootMove(&mainThread->rootMoves[i], ml->moves[i]);

    mainThread->numRootMoves = ml->count;
  }
}

void SetupOtherThreads(Board* board) {
  ThreadData* mainThread = Threads.threads[0];

  for (int i = 1; i < Threads.count; i++) {
    ThreadData* thread = Threads.threads[i];
    thread->calls      = 0;
    thread->nodes      = 0;
    thread->tbhits     = 0;
    thread->seldepth   = 1;

    for (int j = 0; j < mainThread->numRootMoves; j++)
      InitRootMove(&thread->rootMoves[j], mainThread->rootMoves[j].move);

    thread->numRootMoves = mainThread->numRootMoves;

    memcpy(&thread->board, board, offsetof(Board, accumulators));
  }
}

// sum node counts
uint64_t NodesSearched() {
  uint64_t nodes = 0;
  for (int i = 0; i < Threads.count; i++)
    nodes += Threads.threads[i]->nodes;

  return nodes;
}

uint64_t TBHits() {
  uint64_t tbhits = 0;
  for (int i = 0; i < Threads.count; i++)
    tbhits += Threads.threads[i]->tbhits;

  return tbhits;
}
