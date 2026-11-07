#!/usr/bin/env python3
"""Asynchronous Present for the render-thread release (facade + ring).

With the render thread on, Present waited three times for the GL worker: the
overlay ran as a blocking job (the worker had to finish the whole frame's GL
first), the swap was a blocking job (including the display's refresh wait),
and a barrier followed. The game thread could not start frame N+1 before the
worker had submitted and swapped frame N, so a frame cost game CPU + driver CPU
+ swap wait (RG DS main menu: ~25 + ~30 ms).

Now the screen-fit blit + overlay and the swap are queued as copied jobs
(ds_rt_submit), and Present only waits until the previous frame's swap is
done: the game thread runs at most one frame ahead of the worker. The jobs
leave buffer/attribute/unpack state as they found it (the overlay restores
it), as copied jobs must. The timing probe (DS_PRESENT_TIMING) keeps the old
blocking path; DS_RT_ASYNC_PRESENT=0 also restores it.

The streaming ring grows to four segments: at the end of frame G the worker
fences G's segment and waits for the fence of frame G-2, whose segment frame
G+2 will reuse; with at most one frame in flight the game thread never writes
a segment the GPU may still read.

Inputs (pinned): build/gpu-ring/d3d8.c and build/gpu-ring/d3d8_renderer.c.
Output: build/async-present/.
"""
from pathlib import Path
import hashlib, json, shutil

ROOT = Path(__file__).resolve().parents[1]
base = ROOT / 'build/gpu-ring'
out = ROOT / 'build/async-present'
out.mkdir(exist_ok=True)

def pinned(path, digest):
    s = path.read_text()
    assert hashlib.sha256(s.encode()).hexdigest() == digest, ('input changed', path)
    return s

def replace(s, old, new, count=1):
    assert s.count(old) == count, ('anchor', s.count(old), old[:100])
    return s.replace(old, new)

# ---- renderer: four segments, frame step as a copied job -------------------
r = pinned(base / 'd3d8_renderer.c', '8995767dc671afc0996e9807ec8d28efb771d550800c50557c86a7abac8888e8')
r = replace(r, '3u*RING_SEGMENT', '4u*RING_SEGMENT', 2)
r = replace(r, 'void *fence[3];', 'void *fence[4];')
r = replace(r, '"[gpu-ring] streaming ring on (3 x %u MiB, persistent)\\n"', '"[gpu-ring] streaming ring on (4 x %u MiB, persistent)\\n"')
old_step = r[r.index('static void ring_frame_job(void *unused){'):r.index('static int ring_ready(void){')]
new_step = r'''/* End of frame G on the GL thread: fence G's segment, then wait for frame
 * G-2's fence and free its segment (frame G+2 reuses it). Copied job. */
typedef struct {unsigned ended,release;} RingStep;
static void ring_step_job(void *p){
    const RingStep *s=p;
    if(ring.fence[s->ended])ring.delete_fn(ring.fence[s->ended]);
    ring.fence[s->ended]=ring.fence_fn(0x9117 /* SYNC_GPU_COMMANDS_COMPLETE */,0);
    if(ring.fence[s->release]){
        GLenum st=ring.wait_fn(ring.fence[s->release],0,0);
        if(st!=0x911A&&st!=0x911C){__atomic_add_fetch(&ring.waits,1,__ATOMIC_RELAXED);ring.wait_fn(ring.fence[s->release],0x00000001 /* FLUSH */,1000000000ull);}
        ring.delete_fn(ring.fence[s->release]);ring.fence[s->release]=NULL;
    }
}
/* Once per Present, after the frame's draws (facade hook); producer side. */
void d8_renderer_ring_frame(void){
    if(!ring.on)return;
    RingStep s={ring.index,(ring.index+2)%4};
    ds_rt_submit(ring_step_job,&s,sizeof s);   /* runs directly without the worker */
    ring.index=(ring.index+1)%4;ring.pos=(size_t)ring.index*RING_SEGMENT;ring.end=ring.pos+RING_SEGMENT;
    if(++ring.frames%1800==0)fprintf(stderr,"[gpu-ring] frames=%llu draws=%llu fallbacks=%llu waits=%llu\n",ring.frames,ring.draws,ring.fallbacks,__atomic_load_n(&ring.waits,__ATOMIC_RELAXED));
}
'''
r = r.replace(old_step, new_step)
(out / 'd3d8_renderer.c').write_text(r)

# ---- facade: copied overlay + swap jobs, at most one frame in flight -------
f = pinned(base / 'd3d8.c', '5034023249cee1dba2f3c819d0367733cea4a59b4f0ceb77e196ce914b857316')
ASYNC = r'''/* ---- asynchronous Present (scripts/stage_async_present.py) ---- */
static int async_present_on(void){static int on=-1;if(on<0){const char *v=getenv("DS_RT_ASYNC_PRESENT");on=!(v&&!strcmp(v,"0"));}return on&&ds_rt_active();}
typedef struct {int size[2];} AsyncOverlay;
static void async_overlay_job(void *p){const AsyncOverlay *o=p;fit_present(o->size[0],o->size[1]);ds_fps_overlay_draw_dimensions(o->size[0],o->size[1]);}
static unsigned long long async_submitted,async_swapped;
static void async_swap_job(void *p){SDL_GL_SwapWindow(*(SDL_Window *const *)p);__atomic_add_fetch(&async_swapped,1,__ATOMIC_RELEASE);}
/* Queue the swap; return once the previous frame's swap is done. */
static void async_swap(SDL_Window *w){
    thread_check(ds_rt_submit(async_swap_job,&w,sizeof w),"Present swap");
    ++async_submitted;
    while(__atomic_load_n(&async_swapped,__ATOMIC_ACQUIRE)+1<async_submitted){if(ds_rt_error())break;usleep(200);}
    thread_check(ds_rt_error()?-1:0,"Present");
}
/* ---- end asynchronous Present ---- */
'''
f = replace(f, 'static void thread_overlay(SDL_Window *w,DsPresentTiming *timing){',
            ASYNC + 'static void thread_overlay(SDL_Window *w,DsPresentTiming *timing){\n'
            '    if(!timing->enabled&&async_present_on()){AsyncOverlay o;SDL_GL_GetDrawableSize(w,&o.size[0],&o.size[1]);thread_check(ds_rt_submit(async_overlay_job,&o,sizeof o),"overlay");return;}')
f = replace(f, 'SDL_GL_SwapWindow(window);thread_check(ds_rt_sync(),"Present");',
            'if(!timing.enabled&&async_present_on())async_swap(window);else{SDL_GL_SwapWindow(window);thread_check(ds_rt_sync(),"Present");}')
(out / 'd3d8.c').write_text(f)
for p in base.iterdir():
    if p.suffix in {'.c', '.h'} and p.name not in {'d3d8.c', 'd3d8_renderer.c'}:
        shutil.copyfile(p, out / p.name)
(out / 'manifest.json').write_text(json.dumps({p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                               for p in sorted(out.iterdir()) if p.suffix in {'.h', '.c'}}, indent=2) + '\n')
print(out)
