#define _GNU_SOURCE
#include "win32hle.h"
#include "profiler.h"
#include <signal.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <ucontext.h>

/* Optional CPU-time sampling. Disabled unless requested; never used as FPS.
 * Signal-safe fixed buckets keep instrumentation out of the frame loop. */
#define NBUCKET 32768
#define HOST_BUCKET 131072
static volatile sig_atomic_t addresses[NBUCKET],counts[NBUCKET],lost;
static volatile uintptr_t host_addresses[HOST_BUCKET];
static volatile sig_atomic_t host_counts[HOST_BUCKET],host_lost;
static volatile uintptr_t caller_pc[HOST_BUCKET],caller_lr[HOST_BUCKET];
static volatile sig_atomic_t caller_counts[HOST_BUCKET],caller_lost;
static int active,registered_exit;
static void sample(int sig,siginfo_t *info,void *context) {
    (void)sig;(void)info;uint32_t address=g_cur_func;
    unsigned slot=(address>>2)&(NBUCKET-1);
    for(unsigned i=0;i<16;i++,slot=(slot+1)&(NBUCKET-1)) {
        if(!addresses[slot] || (uint32_t)addresses[slot]==address) {
            addresses[slot]=(sig_atomic_t)address;counts[slot]++;goto host;
        }
    }
    lost++;
host:;
    uintptr_t pc=0,lr=0;
#if defined(__aarch64__)
    pc=((ucontext_t*)context)->uc_mcontext.pc;
    lr=((ucontext_t*)context)->uc_mcontext.regs[30];
#elif defined(__x86_64__)
    pc=((ucontext_t*)context)->uc_mcontext.gregs[REG_RIP];
#endif
    if(!pc)return;
    uintptr_t hash=(pc>>2)^(pc>>19);hash*=0x9e3779b1u;
    slot=(unsigned)hash&(HOST_BUCKET-1);
    for(unsigned i=0;i<32;i++,slot=(slot+1)&(HOST_BUCKET-1)){
        if(!host_addresses[slot]||host_addresses[slot]==pc){host_addresses[slot]=pc;host_counts[slot]++;goto caller;}
    }
    host_lost++;
caller:;
    if(!lr)return;
    hash=((pc>>2)^(pc>>19)^(lr>>3)^(lr>>21))*0x9e3779b1u;
    slot=(unsigned)hash&(HOST_BUCKET-1);
    for(unsigned i=0;i<32;i++,slot=(slot+1)&(HOST_BUCKET-1)){
        if(!caller_pc[slot]||(caller_pc[slot]==pc&&caller_lr[slot]==lr)){
            caller_pc[slot]=pc;caller_lr[slot]=lr;caller_counts[slot]++;return;
        }
    }
    caller_lost++;
}
void ds_profiler_stop(void) {
    if(!active)return;active=0;
    struct itimerval timer={{0,0},{0,0}};setitimer(ITIMER_PROF,&timer,NULL);
    char path[PATH_MAX];snprintf(path,sizeof path,"%s/cpu-profile.csv",getenv("DS_ROOT"));
    FILE *f=fopen(path,"w");if(!f)return;
    fputs("guest_function,cpu_samples\n",f);unsigned total=0;
    for(unsigned i=0;i<NBUCKET;i++)if(counts[i]) {fprintf(f,"0x%08x,%d\n",(uint32_t)addresses[i],counts[i]);total+=counts[i];}
    fclose(f);fprintf(stderr,"[cpu-profile] samples=%u lost=%d interval_ms=10 file=%s\n",total,lost,path);
    snprintf(path,sizeof path,"%s/host-cpu-profile.csv",getenv("DS_ROOT"));
    f=fopen(path,"w");if(f){fputs("host_pc,cpu_samples\n",f);for(unsigned i=0;i<HOST_BUCKET;i++)if(host_counts[i])fprintf(f,"0x%lx,%d\n",(unsigned long)host_addresses[i],host_counts[i]);fclose(f);}
    snprintf(path,sizeof path,"%s/host-call-profile.csv",getenv("DS_ROOT"));
    f=fopen(path,"w");if(f){fputs("host_pc,host_lr,cpu_samples\n",f);for(unsigned i=0;i<HOST_BUCKET;i++)if(caller_counts[i])fprintf(f,"0x%lx,0x%lx,%d\n",(unsigned long)caller_pc[i],(unsigned long)caller_lr[i],caller_counts[i]);fclose(f);}
    fprintf(stderr,"[cpu-profile] pc_lr_samples lost=%d\n",caller_lost);
    snprintf(path,sizeof path,"%s/host-profile-maps.txt",getenv("DS_ROOT"));
    FILE *maps=fopen("/proc/self/maps","r");f=fopen(path,"w");
    if(maps&&f){char line[1024];while(fgets(line,sizeof line,maps))fputs(line,f);}
    if(maps)fclose(maps);if(f)fclose(f);
    fprintf(stderr,"[cpu-profile] host_pc_samples lost=%d\n",host_lost);
}
void ds_profiler_start(void) {
    const char *enabled=getenv("DS_CPU_PROFILE");if(!enabled || !strcmp(enabled,"0"))return;
    struct sigaction sa;memset(&sa,0,sizeof sa);sa.sa_sigaction=sample;sa.sa_flags=SA_RESTART|SA_SIGINFO;
    sigemptyset(&sa.sa_mask);sigaction(SIGPROF,&sa,NULL);
    struct itimerval timer={{0,10000},{0,10000}};
    if(!setitimer(ITIMER_PROF,&timer,NULL)) {active=1;if(!registered_exit){atexit(ds_profiler_stop);registered_exit=1;}}
}
