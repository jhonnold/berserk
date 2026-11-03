// Berserk is a UCI compliant chess engine written in C
// Copyright (C) 2026 Jay Honnold

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

#include "tune.h"

#define ASSERT_INT(n, v, lo, hi)   _Static_assert((lo) <= (v) && (v) <= (hi), #n " default outside its range");
#define ASSERT_FLOAT(n, v, lo, hi) // floating comparisons are not integer constant expressions

TUNABLES(ASSERT_INT, ASSERT_FLOAT)

#ifdef SPSA

#define DEFINE_INT(n, v, lo, hi)   int n = (v);
#define DEFINE_FLOAT(n, v, lo, hi) double n = (v);

TUNABLES(DEFINE_INT, DEFINE_FLOAT)

#define ENTRY_INT(n, v, lo, hi)   {#n, 0, &n, #v, #lo, #hi, (v), (lo), (hi)},
#define ENTRY_FLOAT(n, v, lo, hi) {#n, 1, &n, #v, #lo, #hi, (v), (lo), (hi)},

const Tunable TUNABLES_TABLE[] = {TUNABLES(ENTRY_INT, ENTRY_FLOAT)};

const int TUNABLES_COUNT = sizeof(TUNABLES_TABLE) / sizeof(TUNABLES_TABLE[0]);

#endif
