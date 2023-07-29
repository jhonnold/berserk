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

#ifndef FENGEN_H
#define FENGEN_H

#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>

typedef char Fen[128];

typedef struct {
  int eval;
  Fen fen;
} PositionData;

typedef struct {
  size_t n;
  float result;
  PositionData* positions;
} GameData;

typedef struct {
  uint64_t idx, total, capacity;
  pthread_mutex_t mutex;
  Fen* fens;
} Book;

typedef struct {
  char* book;

  int filterDuplicates;
  int evalLimit;

  int randomMoveMin, randomMoveMax, randomMoveCount;
  int randomMpv, randomMpvDiff;

  int writeMin, writeMax;

  int depth;
  uint64_t nodes;
} FenGenParams;

extern FenGenParams fenGenParams;

void Generate(uint64_t total);

#endif
