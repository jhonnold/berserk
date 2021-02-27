#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "types.h"

#ifdef WIN32
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
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

sigset_t mask, omask;

long getTimeMs() {
  struct timeval time;
  gettimeofday(&time, NULL);

  return time.tv_sec * 1000 + time.tv_usec / 1000;
}

void sigterm(int signo) { (void)signo; }

void initSignalHandlers() {
  struct sigaction s;
  s.sa_handler = sigterm;

  sigemptyset(&s.sa_mask);
  s.sa_flags = 0;
  sigaction(SIGTERM, &s, NULL);

  sigemptyset(&mask);
  sigaddset(&mask, SIGTERM);
  sigprocmask(SIG_BLOCK, &mask, &omask);
}

int inputWaiting(SearchParams* params) {
  fd_set readfds;

  struct timespec tv;
  tv.tv_sec = 0;
  tv.tv_nsec = 0;

  FD_ZERO(&readfds);
  FD_SET(fileno(stdin), &readfds);

  int res = pselect(16, &readfds, 0, 0, &tv, &omask);
  if (res < 0 && errno == EINTR) {
    params->stopped = 1;
    params->quit = 1;

    return 0;
  }

  return FD_ISSET(fileno(stdin), &readfds);
}
#endif

void readInput(SearchParams* params) {
  int bytes = 0;
  char in[256], *endc;

#ifndef WIN32
  if (inputWaiting(params)) {
#else
  if (inputWaiting()) {
#endif
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
