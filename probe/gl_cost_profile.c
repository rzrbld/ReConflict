/* Diagnostic relink only. Never add this file to the normal runtime build.
 * Link every name from gl_cost_calls.def with GNU ld --wrap=name. Single GL
 * context/render thread only; aggregate wall CPU time, NOT GPU execution time. */
#define _POSIX_C_SOURCE 200809L
#include <GLES2/gl2.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
#define V(name,args,values) ID_##name,
#define R(type,name,args,values) ID_##name,
#include "gl_cost_calls.def"
#undef V
#undef R
    API_COUNT, MAX_FRAMES=600
};
static const char *const names[]={
#define V(name,args,values) #name,
#define R(type,name,args,values) #name,
#include "gl_cost_calls.def"
#undef V
#undef R
};
typedef struct { uint64_t calls,ns,max_ns; } Cost;
static Cost records[MAX_FRAMES][API_COUNT];
static uint64_t frame_ns[MAX_FRAMES],frame_start,timer_pair_ns;
static unsigned frame,limit;
static int initialized,active,finished;

static uint64_t tick(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
static void record(unsigned id,uint64_t begin){uint64_t ns=tick()-begin;Cost *c=&records[frame][id];c->calls++;c->ns+=ns;if(ns>c->max_ns)c->max_ns=ns;}

/* Call from the GL/render thread at a frame boundary, never directly from a
 * signal handler. SIGUSR1 handlers should set a flag consumed by Present. */
void ds_gl_cost_flush(void){
    if(!initialized||finished||(!frame&&!active))return;
    active=0;finished=1;
    const char *root=getenv("DS_ROOT");if(!root||!*root)root=".";
    char path[4096];if(snprintf(path,sizeof path,"%s/gl-cost.csv",root)>=(int)sizeof path){fprintf(stderr,"[gl-cost] output path too long\n");return;}
    FILE *out=fopen(path,"w");if(!out){perror("[gl-cost] fopen");return;}
    fprintf(out,"# CPU API wall time, not GPU time; %u complete frames; timer_pair_min_ns=%" PRIu64 "; no subtraction\n",frame,timer_pair_ns);
    fprintf(out,"frame,frame_ns,api,calls,total_ns,max_ns\n");
    uint64_t total_ns=0,total_calls=0;
    for(unsigned f=0;f<frame;f++)for(unsigned a=0;a<API_COUNT;a++){Cost *c=&records[f][a];if(c->calls){fprintf(out,"%u,%" PRIu64 ",%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",f+1,frame_ns[f],names[a],c->calls,c->ns,c->max_ns);total_calls+=c->calls;total_ns+=c->ns;}}
    if(fclose(out))perror("[gl-cost] fclose");
    fprintf(stderr,"[gl-cost] %u frames, %" PRIu64 " calls, %.3f ms API wall; pair_min=%" PRIu64 " ns, wrote %s\n",frame,total_calls,total_ns/1e6,timer_pair_ns,path);
}

/* Optional live control: call immediately AFTER the Present marker. Calling
 * begin while already recording is rejected, so a partial window is not lost. */
int ds_gl_cost_begin(unsigned count){
    if(active||!count||count>MAX_FRAMES)return -1;
    initialized=1;finished=0;frame=0;limit=count;
    memset(records,0,sizeof records);memset(frame_ns,0,sizeof frame_ns);
    timer_pair_ns=UINT64_MAX;
    for(unsigned i=0;i<256;i++){uint64_t a=tick(),b=tick();if(b-a<timer_pair_ns)timer_pair_ns=b-a;}
    frame_start=tick();active=1;fprintf(stderr,"[gl-cost] armed %u frames, CPU wall timers enabled\n",limit);return 0;
}

/* First call arms the NEXT full frame. The fixed window ends automatically.
 * DS_GL_COST_FRAMES defaults to120, bounded to600 (~0.85MB static storage).
 * DS_GL_COST=1 required; diagnostic relink disabled still has wrapper overhead. */
void ds_gl_cost_present(void){
    if(!initialized){
        initialized=1;const char *enabled=getenv("DS_GL_COST");if(!enabled||strcmp(enabled,"1"))return;
        const char *n=getenv("DS_GL_COST_FRAMES");char *end=NULL;unsigned long count=n?strtoul(n,&end,10):120;
        if(!count||count>MAX_FRAMES||(n&&(*end||end==n))){fprintf(stderr,"[gl-cost] invalid DS_GL_COST_FRAMES (1..600)\n");return;}
        ds_gl_cost_begin((unsigned)count);return;
    }
    if(!active)return;
    uint64_t now=tick();frame_ns[frame]=now-frame_start;frame_start=now;
    if(++frame==limit)ds_gl_cost_flush();
}

#define V(name,args,values) \
    extern void __real_##name args; \
    void __wrap_##name args { if(!active){__real_##name values;return;} \
        uint64_t begin=tick();__real_##name values;record(ID_##name,begin); }
#define R(type,name,args,values) \
    extern type __real_##name args; \
    type __wrap_##name args { if(!active)return __real_##name values; \
        uint64_t begin=tick();type result=__real_##name values;record(ID_##name,begin);return result; }
#include "gl_cost_calls.def"
#undef V
#undef R
