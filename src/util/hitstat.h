#pragma once
#include "print.h"
#include <x86intrin.h>
/* zero-overhead hitcounter/meancounters
hit(cond) record percent of condition==true
mean(value) record min/average/max of value
*/
//https://github.com/FrozenVoid/hitstat/

#define HIT_COUNT_INTERVAL 1000000000ULL
#define meantypemax(value) _Generic((value),\
long long unsigned int:  UINT64_MAX ,\
long long int: INT64_MAX ,\
uint64_t:  UINT64_MAX  ,\
 int64_t: INT64_MAX,\
uint32_t:  UINT32_MAX  ,\
 int32_t: INT32_MAX,\
uint16_t: UINT16_MAX,\
 int16_t: INT16_MAX   ,\
uint8_t:  UINT8_MAX ,\
 int8_t:  INT8_MAX ,\
  float:  FLT_MAX,\
double:   DBL_MAX ,\
 long double:  LDBL_MAX ,\
default: INT64_MAX  )
#define meantypemin(value) _Generic((value),\
long long unsigned int: 0 ,\
long long int: INT64_MIN ,\
uint64_t: 0,\
 int64_t: INT64_MIN,\
uint32_t: 0,\
 int32_t:  INT32_MIN ,\
uint16_t: 0,\
 int16_t: INT16_MIN  ,\
uint8_t:  0,\
 int8_t:  INT8_MIN,\
  float:   FLT_MIN ,\
double: DBL_MIN,\
 long double: LDBL_MIN ,\
default: 0 )

#define mean(value) ({;\
static uint64_t mean_elapsed=0;\
static size_t meancount=0;\
static long double meansum=0;\
meansum+=value;meancount++;\
static typeof(value) valmin=meantypemax(value);\
static typeof(value) valmax=meantypemin(value);\
valmin=value<valmin?value:valmin;\
valmax=value>valmax?value:valmax;\
size_t curtime= __rdtsc();\
if(curtime-mean_elapsed>HIT_COUNT_INTERVAL){mean_elapsed=curtime;\
long double avg=meansum/meancount;\
fprint(stderr,__FILE__,":",__LINE__,":[",stringify(value),"] ",meancount,"counts\n Min:",valmin,"Avg:",avg,"Max:",valmax,"\n");};0;})

#define hit(cond) ({;\
static uint64_t hit_elapsed=0;\
static size_t hitcount=0;\
static size_t condcount=0;\
;hitcount++;condcount+=!!(cond);\
size_t curtime= __rdtsc();\
if(curtime-hit_elapsed>HIT_COUNT_INTERVAL){hit_elapsed=curtime;\
long double perc=100.0*condcount/hitcount;\
fprint(stderr,__FILE__,":",__LINE__,":(",stringify(cond),") ",hitcount,"counts\n Hits:",condcount,"Hit%:",perc,"\n");};0;})



