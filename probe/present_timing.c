/* Diagnostic only: CPU clocks distinguish execution from blocking wall time.
 * Main owns records/file; worker returns two timestamps through the existing
 * synchronous overlay RPC. This does not sample or touch guest/HLE state. */
#include "present_timing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
static int pt_initialized,pt_enabled;
static FILE *pt_file;
static uint64_t pt_previous_main,pt_previous_worker;
static uint64_t pt_ns(clockid_t clock){struct timespec t;if(clock_gettime(clock,&t))return 0;return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
uint64_t ds_present_cpu_ns(void){return pt_ns(CLOCK_THREAD_CPUTIME_ID);}
static double pt_ms(uint64_t a,uint64_t b){return a&&b>=a?(double)(b-a)/1000000.0:-1.0;}
void ds_present_timing_flush(void){if(pt_file)fflush(pt_file);}
DsPresentTiming ds_present_timing_begin(unsigned frame,double guest_tail_ms){
    if(!pt_initialized){
        pt_initialized=1;const char*s=getenv("DS_PRESENT_TIMING"),*root=getenv("DS_ROOT");
        if(s&&!strcmp(s,"1")&&root){char path[PATH_MAX];
            if(snprintf(path,sizeof path,"%s/present-timing.csv",root)<(int)sizeof path)pt_file=fopen(path,"w");
            if(pt_file){pt_enabled=1;fputs("frame,guest_after_last_draw_ms,input_wall_ms,overlay_wall_ms,overlay_main_cpu_ms,overlay_worker_cpu_ms,swap_and_end_wall_ms,frame_main_cpu_ms,frame_worker_cpu_ms,present_main_cpu_ms\n",pt_file);fflush(pt_file);atexit(ds_present_timing_flush);}
        }
    }
    DsPresentTiming t={0};t.enabled=pt_enabled;
    if(t.enabled){t.frame=frame;t.guest_tail_ms=guest_tail_ms;t.wall_begin=pt_ns(CLOCK_MONOTONIC);t.cpu_begin=ds_present_cpu_ns();}
    return t;
}
void ds_present_timing_input(DsPresentTiming*t){if(t->enabled){t->wall_input=pt_ns(CLOCK_MONOTONIC);t->cpu_input=ds_present_cpu_ns();}}
void ds_present_timing_overlay(DsPresentTiming*t){if(t->enabled){t->wall_overlay=pt_ns(CLOCK_MONOTONIC);t->cpu_overlay=ds_present_cpu_ns();}}
void ds_present_timing_end(DsPresentTiming*t){
    if(!t->enabled)return;
    uint64_t wall_end=pt_ns(CLOCK_MONOTONIC),cpu_end=ds_present_cpu_ns();
    fprintf(pt_file,"%u,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
        t->frame,t->guest_tail_ms,pt_ms(t->wall_begin,t->wall_input),
        pt_ms(t->wall_input,t->wall_overlay),pt_ms(t->cpu_input,t->cpu_overlay),
        pt_ms(t->worker_before,t->worker_after),pt_ms(t->wall_overlay,wall_end),
        pt_ms(pt_previous_main,cpu_end),pt_ms(pt_previous_worker,t->worker_after),pt_ms(t->cpu_begin,cpu_end));
    pt_previous_main=cpu_end;pt_previous_worker=t->worker_after;
    if(t->frame%30==0)fflush(pt_file);
}
