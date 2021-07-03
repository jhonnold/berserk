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

// NOOBPROBE IS WRITTEN BY Terje Kirstihagen (Weiss)
// I take 0 credit for this code.

#include "../board.h"
#include "../types.h"

extern int NOOB_BOOK;
extern int NOOB_DEPTH_LIMIT;
extern int failedQueries;

Move ProbeNoob(Board* board);