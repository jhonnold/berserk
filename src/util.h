#ifndef UTIL_H
#define UTIL_H

#include "types.h"

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#ifndef WIN32
void initSignalHandlers();
#endif

long getTimeMs();
void communicate(SearchParams* params);

#endif