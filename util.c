#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
#include <windows.h>
#else
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include "types.h"

long getTimeMs() {
#ifdef WIN32
  return GetTickCount();
#else
  struct timeval time;
  gettimeofday(&time, NULL);

  return time.tv_sec * 1000 + time.tv_usec / 1000;
#endif
}

int inputWaiting() {
#ifndef WIN32
  fd_set readfds;
  struct timeval tv;
  FD_ZERO(&readfds);
  FD_SET(fileno(stdin), &readfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  select(16, &readfds, 0, 0, &tv);

  return (FD_ISSET(fileno(stdin), &readfds));
#else
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
#endif
}

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
