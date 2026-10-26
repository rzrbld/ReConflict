#define _GNU_SOURCE
#include "profiler.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
uint32_t g_cur_func=0x401000;
extern void ds_profiler_frame(unsigned,uint64_t);
static double now(void){struct timespec t;assert(!clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t));return t.tv_sec+t.tv_nsec*1e-9;}
static void burn(double seconds){double end=now()+seconds;volatile unsigned n=0;while(now()<end)n++;}
static unsigned count(const char *root,unsigned segment){char path[1024],line[128];snprintf(path,sizeof path,"%s/dense-%02u-cpu-profile.csv",root,segment);FILE *f=fopen(path,"r");assert(f);assert(fgets(line,sizeof line,f));unsigned total=0,pc,n;while(fgets(line,sizeof line,f)){assert(sscanf(line,"0x%x,%u",&pc,&n)==2);assert(pc==g_cur_func);total+=n;}fclose(f);return total;}
int main(void){
 char root[]="/tmp/dense-profiler-XXXXXX";assert(mkdtemp(root));setenv("DS_ROOT",root,1);setenv("DS_CPU_PROFILE","dense",1);
 ds_profiler_start();burn(.12);ds_profiler_stop();char ordinary[1024];snprintf(ordinary,sizeof ordinary,"%s/cpu-profile.csv",root);assert(access(ordinary,F_OK));
 ds_profiler_frame(1,1000);burn(.02);ds_profiler_frame(2,1001);burn(.14);ds_profiler_frame(3,950);burn(.05);
 ds_profiler_stop();unsigned first=count(root,1);assert(first>=12&&first<=25);
 ds_profiler_start();burn(.06);ds_profiler_frame(4,899);unsigned resumed=count(root,1);assert(resumed>first);
 ds_profiler_frame(5,0);ds_profiler_frame(6,1200);burn(.10);ds_profiler_frame(7,0);unsigned second=count(root,2);assert(second>=5&&second<=15&&second<resumed);
 for(unsigned segment=3;segment<=8;segment++){ds_profiler_frame(segment*2,1100);burn(.02);ds_profiler_frame(segment*2+1,0);assert(count(root,segment)>0);}
 ds_profiler_frame(30,1200);burn(.02);ds_profiler_frame(31,0);
 char ninth[1024];snprintf(ninth,sizeof ninth,"%s/dense-09-cpu-profile.csv",root);assert(access(ninth,F_OK));
 printf("dense_profiler: PASS first=%u resumed=%u second=%u evidence=%s\n",first,resumed,second,root);
 return 0;
}
