#define _GNU_SOURCE
#include "profiler.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

uint32_t g_cur_func;
static char root[PATH_MAX];
static double cpu_time(void) {
    struct timespec t;assert(!clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t));
    return t.tv_sec+t.tv_nsec*1e-9;
}
static void burn(double seconds) {
    double end=cpu_time()+seconds;volatile uint64_t x=1;
    do {for(unsigned i=0;i<10000;i++)x=x*6364136223846793005ULL+1;}while(cpu_time()<end);
}
static unsigned samples(void) {
    char path[PATH_MAX];assert(snprintf(path,sizeof path,"%s/cpu-profile.csv",root)<(int)sizeof path);
    FILE *f=fopen(path,"r");assert(f);char line[256];assert(fgets(line,sizeof line,f));
    unsigned total=0,address,n;
    while(fgets(line,sizeof line,f)){assert(sscanf(line,"0x%x,%u",&address,&n)==2);assert(address==0x401000);total+=n;}
    assert(!fclose(f));return total;
}
static void host_output(unsigned expected) {
    char path[PATH_MAX],line[512];unsigned count,total=0;unsigned long pc;
    assert(snprintf(path,sizeof path,"%s/host-cpu-profile.csv",root)<(int)sizeof path);
    FILE *f=fopen(path,"r");assert(f);assert(fgets(line,sizeof line,f));
    while(fgets(line,sizeof line,f)){assert(sscanf(line,"0x%lx,%u",&pc,&count)==2);assert(pc&&count);total+=count;}
    assert(!fclose(f));assert(total==expected);
    assert(snprintf(path,sizeof path,"%s/host-profile-maps.txt",root)<(int)sizeof path);
    f=fopen(path,"r");assert(f);assert(fgets(line,sizeof line,f));assert(!fclose(f));
}
static void *worker(void *unused) {
    (void)unused;
    /* Foreign calls must neither stop nor rearm the main thread's sampler. */
    ds_profiler_stop();ds_profiler_start();burn(.4);ds_profiler_stop();return NULL;
}
int main(void) {
    snprintf(root,sizeof root,"%s",getenv("DS_PROFILE_TEST_ROOT")?getenv("DS_PROFILE_TEST_ROOT"):"/tmp/ds-main-profiler-XXXXXX");
    assert(mkdtemp(root));assert(!setenv("DS_ROOT",root,1));
    assert(!setenv("DS_CPU_PROFILE","0",1));ds_profiler_start();burn(.02);ds_profiler_stop();
    char path[PATH_MAX];assert(snprintf(path,sizeof path,"%s/cpu-profile.csv",root)<(int)sizeof path);assert(access(path,F_OK));
    assert(!setenv("DS_CPU_PROFILE","1",1));ds_profiler_start();ds_profiler_stop();assert(access(path,F_OK));
    assert(!setenv("DS_CPU_PROFILE","main",1));g_cur_func=0x401000;
    ds_profiler_start();ds_profiler_start();burn(.35);ds_profiler_stop();ds_profiler_stop();
    unsigned initial=samples();assert(initial>=20&&initial<=50);
    /* A busy renderer-like thread must not advance main-thread CPU samples. */
    ds_profiler_start();pthread_t t;assert(!pthread_create(&t,NULL,worker,NULL));assert(!pthread_join(t,NULL));
    ds_profiler_stop();unsigned idle=samples();assert(idle<=initial+1);
    /* Expire a timer while its signal is blocked. Stop must discard pending
       delivery, then rearm cleanly. Externally raised SIGPROF is not a sample. */
    sigset_t set,old;sigemptyset(&set);sigaddset(&set,SIGPROF);assert(!pthread_sigmask(SIG_BLOCK,&set,&old));
    ds_profiler_start();burn(.05);ds_profiler_stop();assert(!pthread_sigmask(SIG_SETMASK,&old,NULL));
    assert(samples()==idle);assert(!raise(SIGPROF));assert(samples()==idle);
    ds_profiler_start();burn(.2);ds_profiler_stop();unsigned final=samples();assert(final>=idle+12&&final<=idle+30);
    host_output(final);
    FILE *f=fopen(path,"r");assert(f);assert(!fclose(f));
    printf("PASS main CPU samples=%u, worker-only delta=%u, resumed delta=%u; disabled, owner isolation, stop/rearm, pending drain and foreign signals\n",initial,idle-initial,final-idle);
    printf("evidence=%s\n",root);return 0;
}
