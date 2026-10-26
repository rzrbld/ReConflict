#define _GNU_SOURCE
#include "present_timing.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
static pthread_barrier_t begin_barrier,end_barrier;
static DsPresentTiming *job;
static double cpu_now(void){struct timespec t;assert(!clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t));return t.tv_sec+t.tv_nsec*1e-9;}
static void burn(double duration){double end=cpu_now()+duration;while(cpu_now()<end){__asm__ volatile("":::"memory");}}
static void *worker(void *unused){(void)unused;for(unsigned i=0;i<2;i++){
 pthread_barrier_wait(&begin_barrier);job->worker_before=ds_present_cpu_ns();burn(.020);usleep(20000);job->worker_after=ds_present_cpu_ns();pthread_barrier_wait(&end_barrier);
 }return NULL;}
int main(int argc,char **argv){
 (void)argv;char root[]="/tmp/present-timing-XXXXXX";assert(mkdtemp(root));assert(!setenv("DS_ROOT",root,1));
 char path[512];snprintf(path,sizeof path,"%s/present-timing.csv",root);
 if(argc>1){unsetenv("DS_PRESENT_TIMING");DsPresentTiming t=ds_present_timing_begin(1,0);assert(!t.enabled);ds_present_timing_input(&t);ds_present_timing_overlay(&t);ds_present_timing_end(&t);ds_present_timing_flush();assert(access(path,F_OK));assert(!rmdir(root));puts("present timing disabled: PASS");return 0;}
 setenv("DS_PRESENT_TIMING","1",1);
 assert(!pthread_barrier_init(&begin_barrier,NULL,2));assert(!pthread_barrier_init(&end_barrier,NULL,2));pthread_t thread;assert(!pthread_create(&thread,NULL,worker,NULL));
 for(unsigned i=1;i<=2;i++){
  burn(.004);DsPresentTiming t=ds_present_timing_begin(i,1.25);assert(t.enabled);usleep(2000);ds_present_timing_input(&t);
  job=&t;pthread_barrier_wait(&begin_barrier);pthread_barrier_wait(&end_barrier);ds_present_timing_overlay(&t);usleep(3000);ds_present_timing_end(&t);
 }
 assert(!pthread_join(thread,NULL));ds_present_timing_flush();FILE*f=fopen(path,"r");assert(f);char line[512];assert(fgets(line,sizeof line,f));
 for(unsigned expected=1;expected<=2;expected++){
  assert(fgets(line,sizeof line,f));unsigned frame;double d[9];assert(sscanf(line,"%u,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf",&frame,&d[0],&d[1],&d[2],&d[3],&d[4],&d[5],&d[6],&d[7],&d[8])==10);
  assert(frame==expected&&d[0]==1.25);assert(d[1]>=1.0&&d[2]>=35.0&&d[5]>=2.0);
  assert(d[4]>=19.0&&d[4]<35.0);assert(d[3]<d[2]*.5&&d[8]<15.0);
  if(expected==1){assert(d[6]==-1&&d[7]==-1);}else{assert(d[6]>=3.0&&d[6]<20.0);assert(d[7]>=19.0&&d[7]<35.0);}
 }
 assert(!fgets(line,sizeof line,f));fclose(f);assert(!unlink(path));assert(!rmdir(root));pthread_barrier_destroy(&begin_barrier);pthread_barrier_destroy(&end_barrier);
 puts("present timing: PASS (CPU burn versus blocking wall time, persistent worker, frame boundaries)");return 0;
}
