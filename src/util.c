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

#include "types.h"

#ifdef WIN32
#include <windows.h>

long GetTimeMS() { return GetTickCount(); }

#else
#include <stddef.h>
#include <sys/time.h>

long GetTimeMS() {
  struct timeval time;
  gettimeofday(&time, NULL);

  return time.tv_sec * 1000 + time.tv_usec / 1000;
}

#endif
