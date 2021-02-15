#include <assert.h>
#include <stdio.h>
#include <sys/time.h>

#include "attacks.h"
#include "bits.h"
#include "board.h"
#include "movegen.h"
#include "types.h"
#include "uci.h"

int64_t perft(int depth) {
  int64_t nodes = 0;

  moves_t moveList[1];
  generateMoves(moveList);

  if (depth == 1)
    return moveList->count;

  for (int i = 0; i < moveList->count; i++) {
    move_t m = moveList->moves[i];

    makeMove(m);
    nodes += perft(depth - 1);
    undoMove(m);
  }

  return nodes;
}

int64_t perftStart(char *fen, int depth) {
  int64_t nodes = 0;

  parseFen(fen);
  printf("Running performance test...\n");
  printBoard();

  struct timeval stop, start;
  gettimeofday(&start, NULL);

  moves_t moveList[1];
  generateMoves(moveList);

  for (int i = 0; i < moveList->count; i++) {
    move_t m = moveList->moves[i];
    makeMove(m);
    int64_t results = perft(depth - 1);
    printf("%s%s%c: %ld\n", idxToCord[moveStart(m)], idxToCord[moveEnd(m)], movePromo(m) ? pieceChars[movePromo(m)] : ' ', results);
    nodes += results;
    undoMove(m);
  }

  gettimeofday(&stop, NULL);

  int64_t duration = ((stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec);

  printf("Got result of %ld took %.3fms (%lld nps)\n", nodes, duration / 1000.0, nodes * 1000000 / duration);
  return nodes;
}

int main() {
  initAttacks();
  parseFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

  uciLoop();

  return 0;
}
