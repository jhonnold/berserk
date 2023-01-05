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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eval.h"
#include "nn.h"
#include "search.h"
#include "transposition.h"
#include "types.h"
#include "util.h"

ThreadPool threadPool;

void* AlignedMalloc(uint64_t size) {
  void* mem  = malloc(size + ALIGN_ON + sizeof(void*));
  void** ptr = (void**) ((uintptr_t) (mem + ALIGN_ON + sizeof(void*)) & ~(ALIGN_ON - 1));
  ptr[-1]    = mem;
  return ptr;
}

void AlignedFree(void* ptr) {
  free(((void**) ptr)[-1]);
}

void ThreadWaitUntilSleep(ThreadData* thread) {
  pthread_mutex_lock(&thread->mutex);

  while (thread->action != THREAD_SLEEP) pthread_cond_wait(&thread->sleep, &thread->mutex);

  pthread_mutex_unlock(&thread->mutex);

  if (thread->idx == 0) threadPool.searching = 0;
}

void ThreadWait(ThreadData* thread, atomic_uchar* cond) {
  pthread_mutex_lock(&thread->mutex);

  while (!atomic_load(cond)) pthread_cond_wait(&thread->sleep, &thread->mutex);

  pthread_mutex_unlock(&thread->mutex);
}

void ThreadWake(ThreadData* thread, int action) {
  pthread_mutex_lock(&thread->mutex);

  if (action != THREAD_RESUME) thread->action = action;

  pthread_cond_signal(&thread->sleep);
  pthread_mutex_unlock(&thread->mutex);
}

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

void* ThreadInit(void* arg) {
  int i = (intptr_t) arg;

  ThreadData* thread          = calloc(1, sizeof(ThreadData));
  thread->idx                 = i;
  thread->results.depth       = 0;
  thread->accumulators[WHITE] = (Accumulator*) AlignedMalloc(sizeof(Accumulator) * (MAX_SEARCH_PLY + 1));
  thread->accumulators[BLACK] = (Accumulator*) AlignedMalloc(sizeof(Accumulator) * (MAX_SEARCH_PLY + 1));
  thread->refreshTable[WHITE] =
    (AccumulatorKingState*) AlignedMalloc(sizeof(AccumulatorKingState) * 2 * N_KING_BUCKETS);
  thread->refreshTable[BLACK] =
    (AccumulatorKingState*) AlignedMalloc(sizeof(AccumulatorKingState) * 2 * N_KING_BUCKETS);

  ResetRefreshTable(thread->refreshTable);

  pthread_mutex_init(&thread->mutex, NULL);
  pthread_cond_init(&thread->sleep, NULL);

  threadPool.threads[i] = thread;

  pthread_mutex_lock(&threadPool.mutex);
  threadPool.init = 0;
  pthread_cond_signal(&threadPool.sleep);
  pthread_mutex_unlock(&threadPool.mutex);

  ThreadIdle(thread);

  return NULL;
}

void ThreadCreate(int i) {
  pthread_t thread;

  threadPool.init = 1;
  pthread_mutex_lock(&threadPool.mutex);
  pthread_create(&thread, NULL, ThreadInit, (void*) (intptr_t) i);

  while (threadPool.init) pthread_cond_wait(&threadPool.sleep, &threadPool.mutex);
  pthread_mutex_unlock(&threadPool.mutex);

  threadPool.threads[i]->nativeThread = thread;
}

void ThreadDestroy(ThreadData* thread) {
  pthread_mutex_lock(&thread->mutex);
  thread->action = THREAD_EXIT;
  pthread_cond_signal(&thread->sleep);
  pthread_mutex_unlock(&thread->mutex);

  pthread_join(thread->nativeThread, NULL);
  pthread_cond_destroy(&thread->sleep);
  pthread_mutex_destroy(&thread->mutex);

  AlignedFree(thread->accumulators[WHITE]);
  AlignedFree(thread->accumulators[BLACK]);
  AlignedFree(thread->refreshTable[WHITE]);
  AlignedFree(thread->refreshTable[BLACK]);

  free(thread);
}

void ThreadsSetNumber(int n) {
  while (threadPool.count < n) ThreadCreate(threadPool.count++);
  while (threadPool.count > n) ThreadDestroy(threadPool.threads[--threadPool.count]);

  if (n == 0) threadPool.searching = 0;
}

void ThreadsExit() {
  ThreadsSetNumber(0);

  pthread_cond_destroy(&threadPool.sleep);
  pthread_mutex_destroy(&threadPool.mutex);
}

void ThreadsInit() {
  pthread_mutex_init(&threadPool.mutex, NULL);
  pthread_cond_init(&threadPool.sleep, NULL);

  threadPool.count = 1;
  ThreadCreate(0);
}

// sum node counts
uint64_t NodesSearched() {
  uint64_t nodes = 0;
  for (int i = 0; i < threadPool.count; i++) nodes += threadPool.threads[i]->nodes;

  return nodes;
}

uint64_t TBHits() {
  uint64_t tbhits = 0;
  for (int i = 0; i < threadPool.count; i++) tbhits += threadPool.threads[i]->tbhits;

  return tbhits;
}
