#pragma once
#include "print.h"
#include <x86intrin.h>
/* zero-overhead hitcounter/meancounters
hit(cond) record if condition was met
mean(value) record average of value
*/
//https://github.com/FrozenVoid/hitstat/

#define HIT_COUNT_INTERVAL 4000000000ULL
#define mean(value) ({;\
static uint64_t elapsed=0;\
static size_t meancount=0;\
static long double meansum=0;\
meansum+=value;meancount++;\
size_t curtime= __rdtsc();\
if(curtime-elapsed>HIT_COUNT_INTERVAL){elapsed=curtime;\
long double avg=meansum/meancount;\
fprint(stderr,__FILE__,":",__LINE__,":(",stringify(value),")Count:",meancount,"Average:",avg,"\n");};0;})

#define hit(cond) ({;\
static uint64_t elapsed=0;\
static size_t hitcount=0;\
static size_t condcount=0;\
;hitcount++;condcount+=!!(cond);\
size_t curtime= __rdtsc();\
if(curtime-elapsed>HIT_COUNT_INTERVAL){elapsed=curtime;\
double perc=100.0*condcount/hitcount;\
fprint(stderr,__FILE__,":",__LINE__,":(",stringify(cond),") Total:",hitcount,"Hits:",condcount,"Hit%:",perc,"\n");};0;})

