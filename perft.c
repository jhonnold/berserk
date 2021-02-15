#include <stdio.h>
#include <sys/time.h>

#include "board.h"
#include "movegen.h"
#include "types.h"

int64_t perftWorker(int depth) {
  if (depth == 0) return 1;

  moves_t moveList[1];
  generateMoves(moveList);

  if (depth == 1) return moveList->count;

  int64_t nodes = 0;

  for (int i = 0; i < moveList->count; i++) {
    move_t m = moveList->moves[i];

    makeMove(m);
    nodes += perftWorker(depth - 1);
    undoMove(m);
  }

  return nodes;
}

void Perft(int depth) {
  int64_t total = 0;

  printf("\nRunning performance test to depth %d\n\n", depth);
  struct timeval stop, start;
  gettimeofday(&start, NULL);

  moves_t moveList[1];
  generateMoves(moveList);

  for (int i = 0; i < moveList->count; i++) {
    move_t m = moveList->moves[i];

    makeMove(m);
    int64_t nodes = perftWorker(depth - 1);
    undoMove(m);

    printf("%s%s%c: %ld\n", idxToCord[moveStart(m)], idxToCord[moveEnd(m)], movePromo(m) ? pieceChars[movePromo(m)] : ' ', nodes);
    total += nodes;
  }

  gettimeofday(&stop, NULL);

  int64_t duration = ((stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec);
  printf("\nNodes: %lld\n", total);
  printf("Time: %.3fms\n", duration / 1000.0);
  printf("NPS: %lld\n\n", total * 1000000 / duration);
}