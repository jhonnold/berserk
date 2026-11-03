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

#include "thread.h"

#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nn/accumulator.h"
#include "search.h"
#include "transposition.h"
#include "types.h"
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
static void ThreadIdle(ThreadData* thread) {
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
      TTClearPart(thread->idx, Threads.count);
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
static void* ThreadInit(void* arg) {
  int i = (intptr_t) arg;

  ThreadData* thread = calloc(1, sizeof(ThreadData));
  thread->idx        = i;

#if defined(__linux__)
  const size_t alignment = MEGABYTE * 2;
#else
  const size_t alignment = 4096;
#endif

  // Alloc all the necessary accumulators
  thread->accumulators = (Accumulator*) AlignedMalloc(sizeof(Accumulator) * (MAX_SEARCH_PLY + 1), alignment);
  thread->board.accumulators = thread->accumulators;
  thread->board.refreshTable =
    (AccumulatorKingState*) AlignedMalloc(sizeof(AccumulatorKingState) * 2 * 2 * N_KING_BUCKETS, alignment);
  ResetRefreshTable(thread->board.refreshTable);

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
static void ThreadCreate(int i) {
  pthread_t thread;

  // Don't trust the platform default (musl gives 128KB)
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 8 * MEGABYTE);

  Threads.init = 1;
  pthread_mutex_lock(&Threads.mutex);
  pthread_create(&thread, &attr, ThreadInit, (void*) (intptr_t) i);
  pthread_attr_destroy(&attr);

  while (Threads.init)
    pthread_cond_wait(&Threads.sleep, &Threads.mutex);
  pthread_mutex_unlock(&Threads.mutex);

  Threads.threads[i]->nativeThread = thread;
}

// Teardown and free a thread
static void ThreadDestroy(ThreadData* thread) {
  pthread_mutex_lock(&thread->mutex);
  thread->action = THREAD_EXIT;
  pthread_cond_signal(&thread->sleep);
  pthread_mutex_unlock(&thread->mutex);

  pthread_join(thread->nativeThread, NULL);
  pthread_cond_destroy(&thread->sleep);
  pthread_mutex_destroy(&thread->mutex);

  AlignedFree(thread->accumulators);
  AlignedFree(thread->board.refreshTable);

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
void ThreadsExit(void) {
  ThreadsSetNumber(0);

  pthread_cond_destroy(&Threads.sleep);
  pthread_mutex_destroy(&Threads.mutex);
  pthread_mutex_destroy(&Threads.lock);
}

// Start
void ThreadsInit(void) {
  pthread_mutex_init(&Threads.mutex, NULL);
  pthread_mutex_init(&Threads.lock, NULL);
  pthread_cond_init(&Threads.sleep, NULL);

  Threads.count = 1;
  ThreadCreate(0);
}

// Run an action on every thread at once and wait until all are done
static void ThreadsRun(int action) {
  for (int i = 0; i < Threads.count; i++)
    ThreadWake(Threads.threads[i], action);
  for (int i = 0; i < Threads.count; i++)
    ThreadWaitUntilSleep(Threads.threads[i]);
}

void ThreadsClearTT(void) {
  ThreadsRun(THREAD_TT_CLEAR);
}

void ThreadsClearSearch(void) {
  ThreadsRun(THREAD_SEARCH_CLEAR);
}

void ThreadsResetRefreshTables(void) {
  for (int i = 0; i < Threads.count; i++)
    ResetRefreshTable(Threads.threads[i]->board.refreshTable);
}

// sum node counts
uint64_t NodesSearched(void) {
  uint64_t nodes = 0;
  for (int i = 0; i < Threads.count; i++)
    nodes += LoadRlx(Threads.threads[i]->nodes);

  return nodes;
}

uint64_t TBHits(void) {
  uint64_t tbhits = 0;
  for (int i = 0; i < Threads.count; i++)
    tbhits += LoadRlx(Threads.threads[i]->tbhits);

  return tbhits;
}
