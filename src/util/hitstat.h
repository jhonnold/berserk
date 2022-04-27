#include "print.h"
//low overhead hitstat debugging tool v1.0
//https://github.com/FrozenVoid/hitstat/
#define MAX_HITCOUNT 256
static uint64_t hitmap[MAX_HITCOUNT][2]={0};
static long double meansum[MAX_HITCOUNT]={0};
static uint64_t hitsum[MAX_HITCOUNT]={0};
#define mean(counter,num) (meansum[counter]+=num,hitsum[counter]++)
#define hit(counter,cond) (hitmap[counter][0]++,hitmap[counter][1]+=!!(cond))
#define printhits()  for(size_t counter=0;counter<MAX_HITCOUNT;counter++){if(hitmap[counter][0]){print("\nHCounter:",counter," Total:",hitmap[counter][0]," Hits:",hitmap[counter][1],"Hit%:",hitmap[counter][1]*100.0/hitmap[counter][0]);} }puts("");
#define printmeans() for(size_t counter=0;counter<MAX_HITCOUNT;counter++){if(hitsum[counter]){print("\nMCounter:",counter," Total:",hitsum[counter]," Average:",meansum[counter]/hitsum[counter]);} }puts("");
