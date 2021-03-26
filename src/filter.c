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
#include "filter.h"
#include "search.h"
#include "types.h"

const int ss[] = {1, -1};

void FilterAll() {
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

    memcpy(positions[p++].fen, buffer, i * sizeof(char));
    memcpy(positions[p - 1].orig, buffer, sizeof(buffer));
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
    if (positions[i].quiet)
      fputs(positions[i].orig, fp);
  }

  fclose(fp);
}

void* batchFilter(void* arg) {
  BatchFilter* job = (BatchFilter*)arg;

  for (int i = 0; i < job->count; i++)
    quiet(job->positions + i);

  pthread_exit(NULL);
}

void quiet(PotentialQuietFen* p) {
  Board* board = malloc(sizeof(Board));
  parseFen(p->fen, board);

  if (board->checkers)
    return;

  int staticEval = ss[board->side] * Evaluate(board);

  SearchData* data = malloc(sizeof(SearchData));
  data->nodes = 0;
  data->seldepth = 0;
  data->ply = 0;
  memset(data->evals, 0, sizeof(data->evals));
  memset(data->moves, 0, sizeof(data->moves));
  memset(data->killers, 0, sizeof(data->killers));
  memset(data->counters, 0, sizeof(data->counters));
  memset(data->hh, 0, sizeof(data->hh));
  memset(data->bf, 0, sizeof(data->bf));
  data->board = board;

  SearchParams* params = malloc(sizeof(SearchParams));

  int qs = ss[board->side] * quiesce(-CHECKMATE, CHECKMATE, params, data);

  if (abs(staticEval - qs) <= 100) {
    p->quiet = 1;
  } else {
    p->quiet = 0;
  }

  free(data);
  free(params);
  free(board);
}

#endif