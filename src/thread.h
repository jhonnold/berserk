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

#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>
#include <stdatomic.h>

#include "types.h"

typedef struct {
  ThreadData* threads[256];
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
void ThreadIdle(ThreadData* thread);
void* ThreadInit(void* arg);
void ThreadCreate(int i);
void ThreadDestroy(ThreadData* thread);
void ThreadsSetNumber(int n);
void ThreadsExit();
void ThreadsInit();

uint64_t NodesSearched();
uint64_t TBHits();

#endif