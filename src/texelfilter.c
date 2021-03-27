#ifdef TUNE

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bits.h"
#include "board.h"
#include "eval.h"
#include "move.h"
#include "search.h"
#include "texelfilter.h"
#include "types.h"

const int ss[] = {1, -1};

void RunFilter() {
  int n = 0;
  PotentialQuietFen* positions = loadFilteringPositions(&n);

  printf("Filtering from %d positions...\n", n);

  filter(positions, n);
  free(positions);
}

PotentialQuietFen* loadFilteringPositions(int* n) {
  FILE* fp;
  fp = fopen(FILE_PATH, "r");

  if (fp == NULL)
    return NULL;

  PotentialQuietFen* positions = malloc(sizeof(PotentialQuietFen) * 10000000);

  int buffersize = 128;
  char buffer[128];

  int p = 0;
  while (fgets(buffer, buffersize, fp)) {
    int i;
    for (i = 0; i < buffersize; i++)
      if (buffer[i] == 'c')
        break;

    memcpy(positions[p].fen, buffer, i * sizeof(char));
    memcpy(positions[p].result, buffer + i, 16 * sizeof(char));
    positions[p].quiet = 0;

    p++;
  }

  *n = p;
  return positions;
}

void filter(PotentialQuietFen* positions, int n) {
  pthread_t threads[THREADS];
  BatchFilter filters[THREADS];

  int chunkSize = (n / THREADS) + 1;

  for (int t = 0; t < THREADS; t++) {
    filters[t].count = t != THREADS - 1 ? chunkSize : (n - ((THREADS - 1) * chunkSize));
    filters[t].positions = positions + (t * chunkSize);
    filters[t].thread = t;

    pthread_create(&threads[t], NULL, batchFilter, (void*)(filters + t));
  }

  for (int t = 0; t < THREADS; t++)
    pthread_join(threads[t], NULL);

  FILE* fp = fopen(OUTPUT_PATH, "w");
  if (fp == NULL) {
    printf("Unable to save data!\n");
    return;
  }

  for (int i = 0; i < n; i++) {
    if (positions[i].quiet) {
      fprintf(fp, "%s %s", positions[i].fen, positions[i].result);
    }
  }

  fclose(fp);
}

void* batchFilter(void* arg) {
  BatchFilter* job = (BatchFilter*)arg;

  for (int i = 0; i < job->count; i++) {
    if ((i & 255) == 0)
      printf("Thread %2d at position count %6d\n", job->thread, i);

    quiet(job->positions + i);
  }

  pthread_exit(NULL);
}

void quiet(PotentialQuietFen* p) {
  Board* board = malloc(sizeof(Board));
  parseFen(p->fen, board);

  if (board->checkers) {
    free(board);
    return;
  }

  int staticEval = ss[board->side] * Evaluate(board);

  PV* pv = malloc(sizeof(PV));

  SearchData* data = malloc(sizeof(SearchData));
  data->board = board;
  data->nodes = 0;
  data->seldepth = 0;
  data->ply = 0;
  memset(data->evals, 0, sizeof(data->evals));
  memset(data->moves, 0, sizeof(data->moves));
  memset(data->killers, 0, sizeof(data->killers));
  memset(data->counters, 0, sizeof(data->counters));
  memset(data->hh, 0, sizeof(data->hh));
  memset(data->bf, 0, sizeof(data->bf));

  SearchParams* params = malloc(sizeof(SearchParams));
  params->depth = 8;
  params->endTime = INT32_MAX;
  params->stopped = 0;
  params->quit = 0;

  int score = ss[board->side] * negamax(-CHECKMATE, CHECKMATE, 1, params, data, pv);

  if (pv->count) {
    int quiet = 1;

    int i;
    for (i = 0; i < pv->count; i++) {
      Move m = pv->moves[i];

      if (board->checkers || moveCapture(m) || movePromo(m) == QUEEN_WHITE || movePromo(m) == QUEEN_BLACK) {
        quiet = 0;
        break;
      }

      makeMove(pv->moves[i], board);
    }

    if (quiet && abs(staticEval - score) <= 75) {
      p->quiet = 1;
      toFen(p->fen, board);
    }
  }

  free(pv);
  free(data);
  free(params);
  free(board);
}

#endif