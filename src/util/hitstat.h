#pragma once
#include "print.h"
#include <x86intrin.h>
/* zero-overhead hitcounter/meancounters
hit(conditions...) record percent of condition==true for each condition
mean(values...) record min/average/max of values
cost(func(x)) record cost of function calls
examples:
insertion: if(a>mean(d) && hit(a>b)){cost( bigfunc(a));}
will result in `if(a>d && (a>b)){bigfunc(a);}
with (a>b) condition being recorded
and (d) being sampled for min/max/avg
and bigfunc(a) measured cost in CPU cycles
collection:
hit(a>b,b<c,d>b); will record true% of conditions in sequence
mean(a,b,c); will record min/max/avg of a,b,c


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

#define mean1(valuex) ({;__auto_type   value=valuex;\
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
fprint(stderr,__FILE__,":",__LINE__,":[",stringify(valuex),"] ",meancount,"counts\n Min:",valmin,"Avg:",avg,"Max:",valmax,"\n");};value;})

#define hit1(condx) ({;__auto_type  cond=condx;\
static uint64_t hit_elapsed=0;\
static size_t hitcount=0;\
static size_t condcount=0;\
;hitcount++;condcount+=!!(cond);\
size_t curtime= __rdtsc();\
if(curtime-hit_elapsed>HIT_COUNT_INTERVAL){hit_elapsed=curtime;\
long double perc=100.0L*condcount/hitcount;\
fprint(stderr,__FILE__,":",__LINE__,":(",stringify(condx),") ",hitcount,"counts\n Hits:",condcount,"Hit%:",perc,"\n");};cond;})

#define cost1(funcx) ({;\
static uint64_t cost_elapsed=0;\
 size_t start_cycle=__rdtsc();\
;__auto_type  funcres=funcx;\
static size_t total_cycles=0;\
static size_t total_calls=0;\
static typeof(total_cycles) valmin=meantypemax(total_cycles);\
static typeof(total_cycles) valmax=meantypemin(total_cycles);\
 size_t curtime= __rdtsc();\
total_calls++;size_t cyclediff=curtime-start_cycle;\
valmin=cyclediff<valmin?cyclediff:valmin;\
valmax=cyclediff>valmax?cyclediff:valmax;\
total_cycles+=cyclediff;\
if(curtime-cost_elapsed>HIT_COUNT_INTERVAL){cost_elapsed=curtime;\
long double avg=1.0L*total_cycles/total_calls;\
fprint(stderr,__FILE__,":",__LINE__,":{",stringify(funcx),"} ",total_calls,"calls\n Avg:",avg," cycles Min<>Max:",valmin,"<>",valmax,"cycles\n");};funcres;})


#define chainapplyhs_(func,arg...) merge(chainapplyhs_,argcount(arg))(func,arg)
#define chainapplyhs_0(...)
#define chainapplyhs_1(func,a,args...) func(a)
#define chainapplyhs_2(func,a,args...) func(a),chainapplyhs_1(func,args)
#define chainapplyhs_3(func,a,args...) func(a),chainapplyhs_2(func,args)
#define chainapplyhs_4(func,a,args...) func(a),chainapplyhs_3(func,args)
#define chainapplyhs_5(func,a,args...) func(a),chainapplyhs_4(func,args)
#define chainapplyhs_6(func,a,args...) func(a),chainapplyhs_5(func,args)
#define chainapplyhs_7(func,a,args...) func(a),chainapplyhs_6(func,args)
#define chainapplyhs_8(func,a,args...) func(a),chainapplyhs_7(func,args)
#define chainapplyhs_9(func,a,args...) func(a),chainapplyhs_8(func,args)
#define chainapplyhs_10(func,a,args...) func(a),chainapplyhs_9(func,args)
#define chainapplyhs_11(func,a,args...) func(a),chainapplyhs_10(func,args)
#define chainapplyhs_12(func,a,args...) func(a),chainapplyhs_11(func,args)
#define chainapplyhs_13(func,a,args...) func(a),chainapplyhs_12(func,args)
#define chainapplyhs_14(func,a,args...) func(a),chainapplyhs_13(func,args)
#define chainapplyhs_15(func,a,args...) func(a),chainapplyhs_14(func,args)
#define chainapplyhs_16(func,a,args...) func(a),chainapplyhs_15(func,args)
#define chainapplyhs_17(func,a,args...) func(a),chainapplyhs_16(func,args)
#define chainapplyhs_18(func,a,args...) func(a),chainapplyhs_17(func,args)
#define chainapplyhs_19(func,a,args...) func(a),chainapplyhs_18(func,args)
#define chainapplyhs_20(func,a,args...) func(a),chainapplyhs_19(func,args)
#define chainapplyhs_21(func,a,args...) func(a),chainapplyhs_20(func,args)
#define chainapplyhs_22(func,a,args...) func(a),chainapplyhs_21(func,args)
#define chainapplyhs_23(func,a,args...) func(a),chainapplyhs_22(func,args)
#define chainapplyhs_24(func,a,args...) func(a),chainapplyhs_23(func,args)
#define chainapplyhs_25(func,a,args...) func(a),chainapplyhs_24(func,args)
#define chainapplyhs_26(func,a,args...) func(a),chainapplyhs_25(func,args)
#define chainapplyhs_27(func,a,args...) func(a),chainapplyhs_26(func,args)
#define chainapplyhs_28(func,a,args...) func(a),chainapplyhs_27(func,args)
#define chainapplyhs_29(func,a,args...) func(a),chainapplyhs_28(func,args)
#define chainapplyhs_30(func,a,args...) func(a),chainapplyhs_29(func,args)
#define chainapplyhs_31(func,a,args...) func(a),chainapplyhs_30(func,args)
#define chainapplyhs_32(func,a,args...) func(a),chainapplyhs_31(func,args)
#define chainapplyhs_33(func,a,args...) func(a),chainapplyhs_32(func,args)
#define chainapplyhs_34(func,a,args...) func(a),chainapplyhs_33(func,args)
#define chainapplyhs_35(func,a,args...) func(a),chainapplyhs_34(func,args)
#define chainapplyhs_36(func,a,args...) func(a),chainapplyhs_35(func,args)
#define chainapplyhs_37(func,a,args...) func(a),chainapplyhs_36(func,args)
#define chainapplyhs_38(func,a,args...) func(a),chainapplyhs_37(func,args)
#define chainapplyhs_39(func,a,args...) func(a),chainapplyhs_38(func,args)
#define chainapplyhs_40(func,a,args...) func(a),chainapplyhs_39(func,args)
#define chainapplyhs_41(func,a,args...) func(a),chainapplyhs_40(func,args)
#define chainapplyhs_42(func,a,args...) func(a),chainapplyhs_41(func,args)
#define chainapplyhs_43(func,a,args...) func(a),chainapplyhs_42(func,args)
#define chainapplyhs_44(func,a,args...) func(a),chainapplyhs_43(func,args)
#define chainapplyhs_45(func,a,args...) func(a),chainapplyhs_44(func,args)
#define chainapplyhs_46(func,a,args...) func(a),chainapplyhs_45(func,args)
#define chainapplyhs_47(func,a,args...) func(a),chainapplyhs_46(func,args)
#define chainapplyhs_48(func,a,args...) func(a),chainapplyhs_47(func,args)
#define chainapplyhs_49(func,a,args...) func(a),chainapplyhs_48(func,args)
#define chainapplyhs_50(func,a,args...) func(a),chainapplyhs_49(func,args)
#define chainapplyhs_51(func,a,args...) func(a),chainapplyhs_50(func,args)
#define chainapplyhs_52(func,a,args...) func(a),chainapplyhs_51(func,args)
#define chainapplyhs_53(func,a,args...) func(a),chainapplyhs_52(func,args)
#define chainapplyhs_54(func,a,args...) func(a),chainapplyhs_53(func,args)
#define chainapplyhs_55(func,a,args...) func(a),chainapplyhs_54(func,args)
#define chainapplyhs_56(func,a,args...) func(a),chainapplyhs_55(func,args)
#define chainapplyhs_57(func,a,args...) func(a),chainapplyhs_56(func,args)
#define chainapplyhs_58(func,a,args...) func(a),chainapplyhs_57(func,args)
#define chainapplyhs_59(func,a,args...) func(a),chainapplyhs_58(func,args)
#define chainapplyhs_60(func,a,args...) func(a),chainapplyhs_59(func,args)
#define chainapplyhs_61(func,a,args...) func(a),chainapplyhs_60(func,args)
#define chainapplyhs_62(func,a,args...) func(a),chainapplyhs_61(func,args)
#define chainapplyhs_63(func,a,args...) func(a),chainapplyhs_62(func,args)
#define chainapplyhs_64(func,a,args...) func(a),chainapplyhs_63(func,args)


#define hit(args...) chainapplyhs_(hit1,args)
#define mean(args...) chainapplyhs_(mean1,args)

#define cost(args...) chainapplyhs_(cost1,args)
