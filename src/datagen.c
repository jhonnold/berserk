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

#include "datagen.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "movegen.h"
#include "options.h"
#include "random.h"
#include "search.h"
#include "thread.h"
#include "transposition.h"
#include "types.h"
#include "util.h"

#define VERIFICATION_DEPTH 10
#define VERIFICATION_NODES 1000000

#define MAX_OPENING_SCORE 1000

typedef struct {
  char** fens;
  int count;
} Book;

static int LoadBook(Book* book, const char* path) {
  FILE* fp = fopen(path, "r");
  if (!fp)
    return 0;

  int capacity = 1024;
  book->fens   = malloc(capacity * sizeof(char*));
  book->count  = 0;

  char buffer[256];
  while (fgets(buffer, sizeof(buffer), fp)) {
    buffer[strcspn(buffer, "\r\n")] = '\0';
    if (!buffer[0])
      continue;

    if (book->count == capacity)
      book->fens = realloc(book->fens, (capacity *= 2) * sizeof(char*));

    book->fens[book->count++] = strdup(buffer);
  }

  fclose(fp);
  return book->count;
}

static void FreeBook(Book* book) {
  for (int i = 0; i < book->count; i++)
    free(book->fens[i]);

  free(book->fens);
}

static int PlayRandomMoves(Board* board, int n) {
  SimpleMoveList moves;

  for (int i = 0; i <= n; i++) {
    RootMoves(&moves, board);
    if (!moves.count)
      return 0;

    if (i < n)
      MakeMoveUpdate(moves.moves[RandomUInt64() % moves.count], board, 0);
  }

  return 1;
}

static int VerificationSearch(Board* board) {
  ResetLimits();
  Limits.depth = VERIFICATION_DEPTH;
  Limits.nodes = VERIFICATION_NODES;

  ThreadsClearTT();
  ThreadsClearSearch();

  StartSearch(board, 0);
  ThreadWaitUntilSleep(Threads.threads[0]);

  return Threads.threads[0]->rootMoves[0].score;
}

void Genfens(char* args) {
  setbuf(stdout, NULL);

  MINIMAL = 1;

  int n              = 0;
  uint64_t seed      = 0;
  char bookPath[256] = "None";

  char* ptrChar;

  if ((ptrChar = strstr(args, "genfens")))
    n = atoi(ptrChar + 8);

  if ((ptrChar = strstr(args, "seed")))
    seed = strtoull(ptrChar + 5, NULL, 10);

  if ((ptrChar = strstr(args, "book")))
    sscanf(ptrChar + 5, "%255s", bookPath);

  if (n <= 0)
    return;

  SeedRandom(seed);

  Book book     = {NULL, 0};
  int usingBook = strcmp(bookPath, "None") != 0;

  if (usingBook && !LoadBook(&book, bookPath)) {
    printf("info string unable to open book %s\n", bookPath);
    return;
  }

  Board board;
  char fen[128];

  for (int generated = 0; generated < n;) {
    if (usingBook)
      ParseFen(book.fens[RandomUInt64() % book.count], &board);
    else
      ParseFen(START_FEN, &board);

    int randomMoves = usingBook ? 6 + RandomUInt64() % 4 : 8 + RandomUInt64() % 2;

    if (!PlayRandomMoves(&board, randomMoves))
      continue;

    if (abs(VerificationSearch(&board)) > MAX_OPENING_SCORE)
      continue;

    BoardToFen(fen, &board);
    printf("info string genfens %s\n", fen);
    generated++;
  }

  FreeBook(&book);
}
