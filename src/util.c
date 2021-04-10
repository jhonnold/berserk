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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

#ifdef WIN32
#include <fcntl.h>
#include <windows.h>

long GetTimeMS() { return GetTickCount(); }

// Source: VICE
int IsInputWaiting() {
  static int init = 0, pipe;
  static HANDLE inh;
  DWORD dw;

  if (!init) {
    init = 1;
    inh = GetStdHandle(STD_INPUT_HANDLE);
    pipe = !GetConsoleMode(inh, &dw);
    if (!pipe) {
      SetConsoleMode(inh, dw & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
      FlushConsoleInputBuffer(inh);
    }
  }
  if (pipe) {
    if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL))
      return 1;
    return dw;
  } else {
    GetNumberOfConsoleInputEvents(inh, &dw);
    return dw <= 1 ? 0 : dw;
  }
}
#else
#include <poll.h>
#include <sys/time.h>
#include <unistd.h>

long GetTimeMS() {
  struct timeval time;
  gettimeofday(&time, NULL);

  return time.tv_sec * 1000 + time.tv_usec / 1000;
}

int IsInputWaiting() {
  struct pollfd pfd[1];
  pfd->fd = STDIN_FILENO;
  pfd->events = POLLIN;

  return poll(pfd, 1, 0) == 1;
}
#endif

// This is reads/parses input when a search is active
// This DOES block, but if called after `IsInputWaiting`,
// read will return instantly
void ReadInputDuringSearch(SearchParams* params) {
  int bytes = 0;
  char in[256], *endc;

  if (IsInputWaiting()) {
    do {
      bytes = read(fileno(stdin), in, 256);
    } while (bytes < 0);

    endc = strchr(in, '\n');
    if (endc)
      *endc = 0;

    if (strlen(in) > 0) {
      if (!strncmp(in, "quit", 4)) {
        params->stopped = 1;
        params->quit = 1;
      }
      if (!strncmp(in, "stop", 4))
        params->stopped = 1;
    }
  }
}

// Main driver for stopping search at time limit or when
// a given uci command is entered
void Communicate(SearchParams* params) {
  if (params->timeset && GetTimeMS() > params->endTime) {
    params->stopped = 1;
    return;
  }

  ReadInputDuringSearch(params);
}
