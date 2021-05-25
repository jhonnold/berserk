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

#include <stdlib.h>

#include "search.h"
#include "types.h"
#include "util.h"

void UpdateTimeParams(SearchParams* params, Score old, Score n, int expands, int depth) {
  // score ended up outside our window
  if (params->timeset && depth >= 5 && abs(old - n) > WINDOW) {
    // adjust score exponentially based on window expansions
    int percentIncrease = (2 << min(4, expands));

    // score improving isn't as bad as worse, so we cut that in half
    if (n > old)
      percentIncrease /= 2;

    params->timeToSpend = params->timeToSpend * (100 + percentIncrease) / 100;

    if (params->startTime + params->timeToSpend <= params->maxTime)
      params->endTime = params->startTime + params->timeToSpend;
    else
      params->endTime = params->maxTime;
  }
}