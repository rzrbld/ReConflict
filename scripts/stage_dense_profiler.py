#!/usr/bin/env python3
"""Gate the verified main-thread sampler around dense draw sequences."""
from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1];out=root/'build/dense-profiler';out.mkdir(parents=True,exist_ok=True)
p=root/'probe/main_thread_profiler.c';data=p.read_bytes();assert hashlib.sha256(data).hexdigest()=='ddd93cdfc45afafe7e26382eb797e518ab5fd947a9e1cbca89956783dbe7bda7'
s=data.decode();s=s.replace('static int registered_exit;','static int registered_exit;\nstatic char output_prefix[32];\nstatic unsigned dense_segment,dense_first;\nstatic int dense_in_progress;')
for name in ('cpu-profile.csv','host-cpu-profile.csv','host-call-profile.csv','host-profile-maps.txt'):
 old=f'"%s/{name}",getenv("DS_ROOT")';assert s.count(old)==1
 s=s.replace(old,f'"%s/%s{name}",getenv("DS_ROOT"),output_prefix')
start='''void ds_profiler_start(void) {
    const char *enabled=getenv("DS_CPU_PROFILE");
    if(!enabled || !strcmp(enabled,"0"))return;
    if(strcmp(enabled,"main")){fprintf(stderr,"[cpu-profile] diagnostic requires DS_CPU_PROFILE=main; process-wide sampling is disabled\\n");return;}
'''
assert s.count(start)==1;s=s.replace(start,'static void start_sampling(void) {\n')
s+='''
void ds_profiler_start(void){
    const char *enabled=getenv("DS_CPU_PROFILE");
    if(!enabled || !strcmp(enabled,"0"))return;
    if(!strcmp(enabled,"dense")){if(dense_in_progress)start_sampling();return;}
    if(strcmp(enabled,"main")){fprintf(stderr,"[cpu-profile] diagnostic requires main or dense mode\\n");return;}
    start_sampling();
}
void ds_profiler_frame(unsigned frame,uint64_t draws){
    if((pid_t)syscall(SYS_gettid)!=getpid())return;
    const char *enabled=getenv("DS_CPU_PROFILE");
    if(!enabled || strcmp(enabled,"dense"))return;
    if(!dense_in_progress && !active && dense_segment<8 && draws>1000){
        /* Sampler is inactive; no handler can mutate buckets while resetting. */
        memset((void*)addresses,0,sizeof addresses);memset((void*)counts,0,sizeof counts);lost=0;
        memset((void*)host_addresses,0,sizeof host_addresses);memset((void*)host_counts,0,sizeof host_counts);host_lost=0;
        memset((void*)caller_pc,0,sizeof caller_pc);memset((void*)caller_lr,0,sizeof caller_lr);memset((void*)caller_counts,0,sizeof caller_counts);caller_lost=0;
        dense_first=frame+1;dense_in_progress=1;dense_segment++;
        snprintf(output_prefix,sizeof output_prefix,"dense-%02u-",dense_segment);
        fprintf(stderr,"[dense-profile] segment=%u first_frame=%u trigger_draws=%llu\\n",dense_segment,dense_first,(unsigned long long)draws);
        start_sampling();
    }else if(dense_in_progress && draws<900){
        dense_in_progress=0;ds_profiler_stop();
        fprintf(stderr,"[dense-profile] segment=%u frames=%u:%u terminal_draws=%llu (includes first low-draw frame)\\n",dense_segment,dense_first,frame,(unsigned long long)draws);
    }
}
'''
(out/'profiler.c').write_text(s)
p=root/'build/render-thread-game/src/d3d8.c';data2=p.read_bytes();assert hashlib.sha256(data2).hexdigest()=='19ab893bbcd2ea2fcaa93504aaa9d2f70339728ee8324f0ab1e2a0c461af07d6'
s2=data2.decode();anchor='static void trace_present(double swap_ms){';assert s2.count(anchor)==1
s2=s2.replace(anchor,'extern void ds_profiler_frame(unsigned,uint64_t) __attribute__((weak));\n'+anchor)
anchor='    phase_present();render_seen_draw';assert s2.count(anchor)==1
s2=s2.replace(anchor,'    if(ds_profiler_frame)ds_profiler_frame(frames,render_frame.draws);\n'+anchor)
(out/'d3d8.c').write_text(s2)
(out/'inputs.json').write_text(json.dumps({'sampler_input_sha256':hashlib.sha256(data).hexdigest(),'facade_input_sha256':hashlib.sha256(data2).hexdigest(),'scope':'Main-thread CPU sampler starts after >1000-draw frame, stops after <900-draw frame, maximum 8 independently reset segments. Diagnostic only; first low-draw frame included.'},indent=2)+'\n')
