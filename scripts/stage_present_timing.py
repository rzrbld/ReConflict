#!/usr/bin/env python3
"""Add opt-in presentation wall/main/worker CPU clocks to the tested facade."""
from pathlib import Path
import hashlib,shutil
root=Path(__file__).resolve().parents[1];out=root/'build/present-timing';out.mkdir(exist_ok=True)
p=root/'build/dense-profiler/d3d8.c';s=p.read_text()
# Record and validate the exact input stage, without modifying it.
expected=''
manifest=root/'build/render-thread-game/variants/length-scalar/EXPERIMENT-SHA256SUMS'
for line in manifest.read_text().splitlines():
 if line.endswith('  build/dense-profiler/d3d8.c'):expected=line.split()[0]
assert expected and hashlib.sha256(p.read_bytes()).hexdigest()==expected
s='#include "present_timing.c"\n'+s
old='''static void overlay_job(void *argument){
    const int *size=argument;ds_fps_overlay_draw_dimensions(size[0],size[1]);
}
static void thread_overlay(SDL_Window *w){
    if(!ds_rt_active()){ds_fps_overlay_draw(w);return;}
    int size[2];SDL_GL_GetDrawableSize(w,&size[0],&size[1]);
    /* Blocking RPC protects overlay globals from the producer's presented hook. */
    thread_check(ds_rt_call(overlay_job,size),"overlay fence");
}'''
new='''typedef struct {int size[2],timed;uint64_t before,after;} TimedOverlay;
static void overlay_job(void *argument){
    TimedOverlay *job=argument;
    if(job->timed)job->before=ds_present_cpu_ns();
    ds_fps_overlay_draw_dimensions(job->size[0],job->size[1]);
    if(job->timed)job->after=ds_present_cpu_ns();
}
static void thread_overlay(SDL_Window *w,DsPresentTiming *timing){
    if(!ds_rt_active()){ds_fps_overlay_draw(w);return;}
    TimedOverlay job={.timed=timing->enabled};SDL_GL_GetDrawableSize(w,&job.size[0],&job.size[1]);
    /* Existing blocking RPC keeps the reply and overlay globals synchronized. */
    thread_check(ds_rt_call(overlay_job,&job),"overlay fence");
    timing->worker_before=job.before;timing->worker_after=job.after;
}'''
assert s.count(old)==1;s=s.replace(old,new)
old='static void present(void){if(!window){RET(INVALID,5);}ds_input_pump();ds_rt_origin(g_cur_func,frames+1);thread_overlay(window);'
new='''static void present(void){if(!window){RET(INVALID,5);}
double guest_tail_ms=phase_last?(SDL_GetPerformanceCounter()-phase_last)*1000.0/SDL_GetPerformanceFrequency():-1.0;
DsPresentTiming timing=ds_present_timing_begin(frames+1,guest_tail_ms);
ds_input_pump();ds_rt_origin(g_cur_func,frames+1);ds_present_timing_input(&timing);
thread_overlay(window,&timing);ds_present_timing_overlay(&timing);'''
assert s.count(old)==1;s=s.replace(old,new)
old='ds_fps_overlay_presented();frames++;trace_present('
assert s.count(old)==1;s=s.replace(old,'ds_fps_overlay_presented();ds_present_timing_end(&timing);frames++;trace_present(')
old='void ds_graphics_profile_flush(void){'
assert s.count(old)==1;s=s.replace(old,old+'ds_present_timing_flush();')
(out/'d3d8.c').write_text(s)
for name in ['present_timing.c','present_timing.h']:shutil.copy(root/'probe'/name,out/name)
print('present timing facade',hashlib.sha256(s.encode()).hexdigest())
