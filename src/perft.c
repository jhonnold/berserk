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

#include <inttypes.h>
#include <stdio.h>

#include "board.h"
#include "move.h"
#include "movegen.h"
#include "movepick.h"
#include "types.h"
#include "util.h"

uint64_t Perft(int depth, Board* board) {
  if (depth == 0) return 1;

  Move move;
  MoveList moves;
  InitPerftMoves(&moves, board);

  if (depth == 1) return moves.nTactical + moves.nQuiets;

  uint64_t nodes = 0;
  while ((move = NextMove(&moves, board, 0))) {
    MakeMoveUpdate(move, board, 0);
    nodes += Perft(depth - 1, board);
    UndoMove(move, board);
  }

  return nodes;
}

void PerftTest(int depth, Board* board) {
  uint64_t total = 0;

  printf("\nRunning performance test to depth %d\n\n", depth);

  long startTime = GetTimeMS();

  Move move;
  MoveList moves;
  InitPerftMoves(&moves, board);

  while ((move = NextMove(&moves, board, 0))) {
    MakeMoveUpdate(move, board, 0);
    uint64_t nodes = Perft(depth - 1, board);
    UndoMove(move, board);

    printf("%-5s: %" PRIu64 "\n", MoveToStr(move, board), nodes);
    total += nodes;
  }

  long endTime = GetTimeMS();

  printf("\nNodes: %" PRIu64 "\n", total);
  printf("Time: %ldms\n", (endTime - startTime));
  printf("NPS: %" PRIu64 "\n\n", total / max(1, (endTime - startTime)) * 1000);
}