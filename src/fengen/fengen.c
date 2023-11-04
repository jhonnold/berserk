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

#include "fengen.h"

#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include "../board.h"
#include "../move.h"
#include "../nn/accumulator.h"
#include "../nn/evaluate.h"
#include "../pyrrhic/tbprobe.h"
#include "../random.h"
#include "../search.h"
#include "../tb.h"
#include "../thread.h"
#include "../transposition.h"
#include "../uci.h"
#include "../util.h"

#define DUPLICATE_HASH_SIZE (64 * 1024 * 1024)
uint64_t DUPLICATE_HASH[DUPLICATE_HASH_SIZE];

FenGenParams fenGenParams;
Book* book = NULL;

enum {
  STM_LOSS = 1,
  DRAW     = 2,
  STM_WIN  = 3,
};

volatile int STOPPED = 0;

static void SigintHandler() {
  printf("Ending games...\n");
  STOPPED = 1;
}

char* NextPosition() {
  pthread_mutex_lock(&book->mutex);
  char* fen = book->fens[book->idx++];
  if (book->idx >= book->total)
    book->idx = 0;
  pthread_mutex_unlock(&book->mutex);

  return fen;
}

void DetermineRandomMoves(uint8_t* randoms) {
  uint64_t size = fenGenParams.randomMoveMax - fenGenParams.randomMoveMin + 1;
  int* numbers  = malloc(sizeof(int) * size);

  // Reset flags
  for (int i = fenGenParams.randomMoveMin - 1; i < fenGenParams.randomMoveMax; i++)
    randoms[i] = 0;

  for (int i = fenGenParams.randomMoveMin - 1, j = 0; i < fenGenParams.randomMoveMax; i++, j++)
    numbers[j] = i;

  for (int i = 0; i < fenGenParams.randomMoveCount; i++) {
    uint64_t randomIdx = (RandomUInt64() % (size - i)) + i;
    int temp           = numbers[randomIdx];
    numbers[randomIdx] = numbers[i];
    numbers[i]         = temp;

    randoms[numbers[i]] = 1;
  }

  free(numbers);
}

int SeenBefore(uint64_t zobrist) {
  uint64_t idx  = (uint64_t) (zobrist & (DUPLICATE_HASH_SIZE - 1));
  uint64_t curr = DUPLICATE_HASH[idx];

  if (curr == zobrist) {
    return 1;
  } else {
    DUPLICATE_HASH[idx] = zobrist;
    return 0;
  }
}

int GameResult(Board* board, int* scores, int ply) {
  if (ply >= fenGenParams.writeMax || IsFiftyMoveRule(board) || IsMaterialDraw(board) || IsRepetition(board, 0))
    return DRAW;

  SimpleMoveList moves[1];
  RootMoves(moves, board);
  if (!moves->count)
    return board->checkers ? STM_LOSS : DRAW;

  if (ply >= 80) {
    for (int cons = 0, i = ply - 1; i >= 0; i--) {
      if (abs(scores[i]) <= 2) {
        if (++cons >= 8)
          return DRAW;
      } else
        break;
    }
  }

  return 0;
}

void* PlayGames(void* arg) {
  int idx = (intptr_t) arg;

  ThreadData* thread = Threads.threads[idx];
  Board* board       = malloc(sizeof(Board));

  char filename[64];
  sprintf(filename, "%s/berserk" VERSION "_%d.fens", fenGenParams.dir,idx);

  FILE* fp = fopen(filename, "a");
  if (fp == NULL)
    return NULL;

  int* scores     = malloc(sizeof(int) * (fenGenParams.writeMax + 1));
  GameData* game  = malloc(sizeof(GameData));
  game->positions = malloc(sizeof(PositionData) * (fenGenParams.writeMax + 1));

  uint8_t* randoms = malloc(sizeof(uint8_t) * fenGenParams.randomMoveMax);

  while (!STOPPED) {
    SearchClearThread(thread);
    DetermineRandomMoves(randoms);

    ClearBoard(board);
    if (book) {
      char* next = NextPosition();
      ParseFen(next, board);
    } else {
      ParseFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", board);
    }

    game->n = 0;

    int result = 0, resign = 0, ply;
    for (ply = 0;; ply++) {
      result = GameResult(board, scores, ply);
      if (result)
        break;

      FixedSeach(thread, board, fenGenParams.nodes, fenGenParams.depth, 1);
      int score = scores[ply] = thread->rootMoves[0].score;

      if (abs(score) >= fenGenParams.evalLimit) {
        resign++;
        if (resign >= 4 || abs(score) >= TB_WIN_BOUND) {
          result = score > 0 ? STM_WIN : STM_LOSS;
          break;
        }
      } else
        resign = 0;

      Move bestMove = thread->rootMoves[0].move;

      if ( !(ply < fenGenParams.writeMin
          || IsCap(bestMove)
          || board->checkers
          || (CHESS_960 && IsCas(bestMove))
          || (fenGenParams.filterDuplicates && SeenBefore(board->zobrist)))) {

        PositionData* p = &game->positions[game->n++];
        BoardToFen(p->fen, board);
        p->eval = board->stm == WHITE ? score : -score;
      }

      // Make it a random move if needed
      if (ply < fenGenParams.randomMoveMax && randoms[ply]) {
        if (fenGenParams.randomMpv > 0) {
          FixedSeach(thread, board, fenGenParams.nodes, fenGenParams.depth, fenGenParams.randomMpv);

          uint64_t possible = Min(thread->numRootMoves, fenGenParams.randomMpv);
          for (uint64_t i = 1; i < possible; i++) {
            if (thread->rootMoves[0].score > thread->rootMoves[i].score + fenGenParams.randomMpvDiff) {
              possible = i;
              break;
            }
          }

          bestMove = thread->rootMoves[RandomUInt64() % possible].move;
        } else {
          bestMove = thread->rootMoves[RandomUInt64() % thread->numRootMoves].move;
        }
      }

      MakeMoveUpdate(bestMove, board, 0);
      if (board->fmr == 0)
        board->histPly = 0;
    }

    if (result) {
      float fResult = result == STM_WIN ? 1.0 : result == DRAW ? 0.5 : 0.0;
      if (board->stm == BLACK)
        fResult = 1.0 - fResult;

      for (size_t i = 0; i < game->n; i++) {
        fputs(game->positions[i].fen, fp);
        fprintf(fp, " [%1.1f] %d\n", fResult, game->positions[i].eval);
      }
      thread->fens += game->n;
    }
  }

  fclose(fp);
  return NULL;
}

void Generate(uint64_t total) {
  signal(SIGINT, SigintHandler);

  int hashSize = 40.96 * Threads.count;
  TTInit(hashSize);
  printf("Initiating hash table to size: %d\n", hashSize);

  if (fenGenParams.book) {
    FILE* fbook = fopen(fenGenParams.book, "r");
    if (fbook == NULL) {
      printf("Unable to open %s\n", fenGenParams.book);
      exit(EXIT_FAILURE);
    }

    book = calloc(sizeof(Book), 1);
    pthread_mutex_init(&book->mutex, NULL);
    book->idx = book->total = 0;
    book->capacity          = 4096;
    book->fens              = calloc(sizeof(Fen), book->capacity);

    while (fgets(book->fens + book->total, sizeof(Fen), fbook) != NULL) {
      book->total++;
      if (book->total >= book->capacity) {
        book->capacity += 4096;
        book->fens = realloc(book->fens, sizeof(Fen) * book->capacity);
      }
    }

    printf("Successfully loaded book with %lld positions\n", book->total);
  }

  long startTime = GetTimeMS();

  for (int i = 0; i < Threads.count; i++)
    pthread_create(&Threads.threads[i]->nativeThread, NULL, PlayGames, (void*) (intptr_t) i);

  uint64_t generated = 0;
  while (!STOPPED) {
    generated = 0;
    for (int i = 0; i < Threads.count; i++)
      generated += Threads.threads[i]->fens;

    long elapsed = GetTimeMS() - startTime;
    printf("Generated: %10lld [%6.2f/s] [%6lds]\n", generated, 1000.0 * generated / elapsed, elapsed / 1000);

    if (generated >= total)
      break;

    SleepInSeconds(5);
  }

  STOPPED = 1;

  for (int i = 0; i < Threads.count; i++)
    pthread_join(Threads.threads[i]->nativeThread, NULL);

  if (book) {
    pthread_mutex_destroy(&book->mutex);
    free(book);
  }
}