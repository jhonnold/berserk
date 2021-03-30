#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

#ifdef WIN32
#include <fcntl.h>
#include <windows.h>

long getTimeMs() { return GetTickCount(); }

int inputWaiting() {
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

long getTimeMs() {
  struct timeval time;
  gettimeofday(&time, NULL);

  return time.tv_sec * 1000 + time.tv_usec / 1000;
}

int inputWaiting() {
  struct pollfd pfd[1];
  pfd->fd = STDIN_FILENO;
  pfd->events = POLLIN;

  return poll(pfd, 1, 0) == 1;
}
#endif

void readInput(SearchParams* params) {
  int bytes = 0;
  char in[256], *endc;

  if (inputWaiting()) {
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

void communicate(SearchParams* params) {
  if (params->timeset && getTimeMs() > params->endTime) {
    params->stopped = 1;
    return;
  }

  readInput(params);
}
