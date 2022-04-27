#include "print.h"
/* syntax:
hit(counter_num, condition) add a hit
count if condition is met,total++
mean(counter_num2,value) add value to
counter sum,record total++ value count
printhits() and printmeans() will
print all used counters for hits/means
*/
//https://github.com/FrozenVoid/hitstat/
#define MAX_HITCOUNT 256
static uint64_t __attribute__((unused)) hitmap[MAX_HITCOUNT][2]={0};
static long double __attribute__((unused)) meansum[MAX_HITCOUNT]={0};
static uint64_t __attribute__((unused)) hitsum[MAX_HITCOUNT]={0};
static uint8_t __attribute__((unused)) meanset_flag=0,__attribute__((unused)) hitset_flag=0;

#define mean(counter,num) (meansum[counter]+=num,hitsum[counter]++,meanset_flag=1)
#define hit(counter,cond) (hitmap[counter][0]++,hitmap[counter][1]+=!!(cond),hitset_flag=1)
#define printhits() if(hitset_flag){ for(size_t counter=0;counter<MAX_HITCOUNT;counter++){if(hitmap[counter][0]){print("\nHCounter:",counter," Total:",hitmap[counter][0]," Hits:",hitmap[counter][1],"Hit%:",hitmap[counter][1]*100.0/hitmap[counter][0]);} }puts("");}

#define printmeans() if(meanset_flag){ for(size_t counter=0;counter<MAX_HITCOUNT;counter++){if(hitsum[counter]){print("\nMCounter:",counter," Total:",hitsum[counter]," Average:",meansum[counter]/hitsum[counter]);} }puts("");}
