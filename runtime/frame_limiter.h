#ifndef DS_FRAME_LIMITER_H
#define DS_FRAME_LIMITER_H
/* Presentation pacing only. The guest's clock, AI and audio timers are unchanged. */
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
static inline int ds_frame_cap_value(void){
    static int initialized,cap=-1;
    if(!initialized){
        initialized=1;const char *s=getenv("DS_FRAME_CAP");
        if(s){
            if(!strcmp(s,"0"))cap=0;
            else if(!strcmp(s,"30"))cap=30;
            else if(!strcmp(s,"60"))cap=60;
            else fprintf(stderr,"[frame-cap] invalid DS_FRAME_CAP; retaining game presentation policy\n");
        }
        fprintf(stderr,"[frame-cap] %s%d\n",cap<0?"game policy ":cap==0?"uncapped ":"limit ",cap);
    }
    return cap;
}
static inline int ds_frame_swap_interval(int game_interval){
    int cap=ds_frame_cap_value();return cap<0?game_interval:cap==0?0:1;
}
static inline uint64_t ds_frame_clock_ns(void){
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint64_t)t.tv_sec*1000000000ull+(uint64_t)t.tv_nsec;
}
static inline void ds_frame_limit_present(void){
    static uint64_t previous;
    int cap=ds_frame_cap_value();if(cap<=0){previous=0;return;}
    uint64_t now=ds_frame_clock_ns();
    uint64_t deadline=0;
    if(previous){
        deadline=previous+(1000000000ull+(unsigned)cap-1)/(unsigned)cap;
        if(now<deadline){
            struct timespec until={(time_t)(deadline/1000000000ull),(long)(deadline%1000000000ull)};
            while(clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&until,NULL)==EINTR){}
            now=ds_frame_clock_ns();
        }
    }
    /* Keep ordinary scheduler jitter from drifting against VSync. A late
     * frame/load/pause resets the schedule instead of accumulating debt. */
    previous=deadline&&now>=deadline&&now-deadline<=1000000ull?deadline:now;
}
#endif
