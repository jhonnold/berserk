// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2021 Jay Honnold

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

#include <string.h>

#include "attacks.h"
#include "bench.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "random.h"
#include "search.h"
#include "texel.h"
#include "transposition.h"
#include "types.h"
#include "uci.h"
#include "util.h"
#include "zobrist.h"

// Welcome to berserk
int main(int argc, char** argv) {
  SeedRandom(0);

  InitPSQT();
  InitAttacks();
  InitZobristKeys();
  InitPruningAndReductionTables();

  TTInit(32);

  // Compliance for OpenBench
  if (argc > 1 && !strncmp(argv[1], "bench", 5)) {
    Bench();
  }
#ifdef TUNE
  else if (argc > 1 && !strncmp(argv[1], "tune", 4)) {
    Texel();
  }
#endif
  else {
    UCILoop();
  }

  TTFree();
  return 0;
}
