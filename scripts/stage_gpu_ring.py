#!/usr/bin/env python3
"""GPU streaming ring for the render-thread release (renderer + facade hook).

Per-draw vertex/index uploads used glBufferData on the renderer's stream
buffers (~100 calls per frame in the main menu). Mesa Panfrost spends ~40 us
of CPU in each (measured with the GL cost probe on RG DS); Mali is cheaper but
not free. As in The ReThing, a persistent coherent buffer of three segments
replaces them: each draw copies its vertices/indices into the current frame's
segment and draws from offsets; each Present fences the segment and the next
one is reused only after its fence signalled (three frames later).

Render thread: the producer copies into the mapped memory before the draw is
queued, so the worker always sees the data. Setup and the per-frame fence
run on the GL thread through ds_rt_call (directly when the worker is off);
glBufferStorageEXT/glMapBufferRange/fence calls come from SDL and are never
called from the producer. No GL_EXT_buffer_storage or a failed map: the ring
stays off and the old upload path is used. DS_GPU_RING=0 disables it.

Inputs (pinned): build/index-scan/d3d8_renderer.c and build/screen-fit/d3d8.c.
Output: build/gpu-ring/ (renderer, facade and the facade's included files).
"""
from pathlib import Path
import hashlib, json, shutil

ROOT = Path(__file__).resolve().parents[1]
out = ROOT / 'build/gpu-ring'
out.mkdir(exist_ok=True)

def pinned(path, digest):
    s = path.read_text()
    assert hashlib.sha256(s.encode()).hexdigest() == digest, ('input changed', path)
    return s

def replace(s, old, new):
    assert s.count(old) == 1, ('anchor', old[:100])
    return s.replace(old, new)

# ---- renderer --------------------------------------------------------------
r = pinned(ROOT / 'build/index-scan/d3d8_renderer.c',
           '1b89c061f98731d92d3fe2f7860354435294f589de263c51ce545a986b49e21e')
RING = r'''
/* ---- GPU streaming ring (scripts/stage_gpu_ring.py) ---- */
typedef void (*RingStorageFn)(GLenum,GLsizeiptr,const void*,GLbitfield);
typedef void *(*RingMapFn)(GLenum,GLintptr,GLsizeiptr,GLbitfield);
typedef void *(*RingFenceFn)(GLenum,GLbitfield);
typedef GLenum (*RingWaitFn)(void*,GLbitfield,unsigned long long);
typedef void (*RingDeleteFn)(void*);
#define RING_SEGMENT (4u<<20)
static struct {
    int tried,on;GLuint buffer;unsigned char *map;unsigned index;size_t pos,end;void *fence[3];
    RingFenceFn fence_fn;RingWaitFn wait_fn;RingDeleteFn delete_fn;
    unsigned long long draws,fallbacks,waits,frames;
} ring;
static size_t ring_index_offset;   /* index offset of the current draw (0: not in the ring) */
static int ring_vertex_draw;       /* the current draw's vertices are in the ring */
static void ring_run(void (*fn)(void*)){if(ds_rt_active())ds_rt_call(fn,NULL);else fn(NULL);}
static void ring_init_job(void *unused){
    (void)unused;
    const char *ext=(const char*)glGetString(GL_EXTENSIONS);
    if(!ext||!strstr(ext,"GL_EXT_buffer_storage")){fprintf(stderr,"[gpu-ring] off: no GL_EXT_buffer_storage\n");return;}
    RingStorageFn storage=(RingStorageFn)SDL_GL_GetProcAddress("glBufferStorageEXT");
    RingMapFn map=(RingMapFn)SDL_GL_GetProcAddress("glMapBufferRange");
    ring.fence_fn=(RingFenceFn)SDL_GL_GetProcAddress("glFenceSync");
    ring.wait_fn=(RingWaitFn)SDL_GL_GetProcAddress("glClientWaitSync");
    ring.delete_fn=(RingDeleteFn)SDL_GL_GetProcAddress("glDeleteSync");
    if(!storage||!map||!ring.fence_fn||!ring.wait_fn||!ring.delete_fn){fprintf(stderr,"[gpu-ring] off: entry points missing\n");return;}
    while(glGetError()!=GL_NO_ERROR){}
    GLuint b=0;glGenBuffers(1,&b);glBindBuffer(GL_ARRAY_BUFFER,b);
    const GLbitfield flags=0x0002u|0x0040u|0x0080u;   /* MAP_WRITE | PERSISTENT_EXT | COHERENT_EXT */
    storage(GL_ARRAY_BUFFER,(GLsizeiptr)(3u*RING_SEGMENT),NULL,flags);
    void *m=glGetError()==GL_NO_ERROR?map(GL_ARRAY_BUFFER,0,(GLsizeiptr)(3u*RING_SEGMENT),flags):NULL;
    if(!m||glGetError()!=GL_NO_ERROR){glBindBuffer(GL_ARRAY_BUFFER,0);glDeleteBuffers(1,&b);while(glGetError()!=GL_NO_ERROR){}fprintf(stderr,"[gpu-ring] off: map failed\n");return;}
    glBindBuffer(GL_ARRAY_BUFFER,0);
    ring.buffer=b;ring.map=m;ring.index=0;ring.pos=0;ring.end=RING_SEGMENT;ring.on=1;
    fprintf(stderr,"[gpu-ring] streaming ring on (3 x %u MiB, persistent)\n",RING_SEGMENT>>20);
}
static void ring_frame_job(void *unused){
    (void)unused;
    unsigned i=ring.index;
    if(ring.fence[i])ring.delete_fn(ring.fence[i]);
    ring.fence[i]=ring.fence_fn(0x9117 /* SYNC_GPU_COMMANDS_COMPLETE */,0);
    i=(i+1)%3;ring.index=i;
    if(ring.fence[i]){
        GLenum st=ring.wait_fn(ring.fence[i],0,0);
        if(st!=0x911A&&st!=0x911C){ring.waits++;ring.wait_fn(ring.fence[i],0x00000001 /* FLUSH */,1000000000ull);}
        ring.delete_fn(ring.fence[i]);ring.fence[i]=NULL;
    }
    ring.pos=(size_t)i*RING_SEGMENT;ring.end=ring.pos+RING_SEGMENT;
}
/* Once per Present, after the frame's draws (facade hook). */
void d8_renderer_ring_frame(void){
    if(!ring.on)return;
    ring_run(ring_frame_job);
    if(++ring.frames%1800==0)fprintf(stderr,"[gpu-ring] frames=%llu draws=%llu fallbacks=%llu waits=%llu\n",ring.frames,ring.draws,ring.fallbacks,ring.waits);
}
static int ring_ready(void){
    if(!ring.tried){ring.tried=1;const char *e=getenv("DS_GPU_RING");if(!(e&&!strcmp(e,"0")))ring_run(ring_init_job);}
    return ring.on;
}
/* Room for bytes at the given alignment in the current segment; offset or (size_t)-1. */
static size_t ring_reserve(size_t bytes,size_t align){
    size_t at=(ring.pos+align-1)/align*align;
    if(at+bytes>ring.end){ring.fallbacks++;return (size_t)-1;}
    ring.pos=at+bytes;return at;
}
/* ---- end GPU streaming ring ---- */
'''
r = replace(r, '#include "index_scan.h"\n', '#include "index_scan.h"\n#include <SDL.h>\n' + RING)
# vertices: copy into the ring and draw from its offset
r = replace(r,
    'else{glBindBuffer(GL_ARRAY_BUFFER,r->gpu_vbo);glBufferData(GL_ARRAY_BUFFER,vertex_bytes,(const char*)data+((size_t)base+first)*stride,GL_STREAM_DRAW);d8_renderer_note_buffer_upload(r,0,vertex_bytes);}',
    'else{size_t at=ring_ready()?ring_reserve(vertex_bytes,16):(size_t)-1;'
    'if(at!=(size_t)-1){memcpy(ring.map+at,(const char*)data+((size_t)base+first)*stride,vertex_bytes);glBindBuffer(GL_ARRAY_BUFFER,ring.buffer);attribute_base=at;ring_vertex_draw=1;ring.draws++;}'
    'else{ring_vertex_draw=0;glBindBuffer(GL_ARRAY_BUFFER,r->gpu_vbo);glBufferData(GL_ARRAY_BUFFER,vertex_bytes,(const char*)data+((size_t)base+first)*stride,GL_STREAM_DRAW);}'
    'd8_renderer_note_buffer_upload(r,0,vertex_bytes);}')
r = replace(r,
    'attributes_begin(r);GLuint attribute_buffer=native&&native->vertex?native->vertex:r->gpu_vbo;',
    'attributes_begin(r);GLuint attribute_buffer=native&&native->vertex?native->vertex:(ring_vertex_draw?ring.buffer:r->gpu_vbo);')
# indices
r = replace(r,
    'else{glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,r->gpu_ibo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,(size_t)n*isize,uploaded_indices,GL_STREAM_DRAW);d8_renderer_note_buffer_upload(r,1,(size_t)n*isize);}',
    'else{size_t at=ring.on?ring_reserve((size_t)n*isize,4):(size_t)-1;'
    'if(at!=(size_t)-1){memcpy(ring.map+at,uploaded_indices,(size_t)n*isize);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ring.buffer);ring_index_offset=at;}'
    'else{glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,r->gpu_ibo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,(size_t)n*isize,uploaded_indices,GL_STREAM_DRAW);ring_index_offset=0;}'
    'd8_renderer_note_buffer_upload(r,1,(size_t)n*isize);}')
r = replace(r, '(void*)(native&&native->index?native->index_offset:0)',
               '(void*)(native&&native->index?native->index_offset:ring_index_offset)')
(out / 'd3d8_renderer.c').write_text(r)

# ---- facade: one ring step per Present, before the overlay/swap ----------
f = pinned(ROOT / 'build/screen-fit/d3d8.c',
           'c78902a6450a36eb6c105c7cc09a1d5476913282d2c5cc6c6b9df85a6e28b6c5')
f = replace(f, 'static void present(void){if(!window){RET(INVALID,5);}',
               'void d8_renderer_ring_frame(void);   /* scripts/stage_gpu_ring.py */\n'
               'static void present(void){if(!window){RET(INVALID,5);}d8_renderer_ring_frame();')
(out / 'd3d8.c').write_text(f)
shutil.copyfile(ROOT / 'build/index-scan/index_scan.h', out / 'index_scan.h')   # the renderer's local include
for p in (ROOT / 'build/screen-fit').iterdir():
    if p.suffix in {'.c', '.h'} and p.name != 'd3d8.c':
        shutil.copyfile(p, out / p.name)
(out / 'manifest.json').write_text(json.dumps({p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                               for p in sorted(out.iterdir()) if p.suffix in {'.h', '.c'}}, indent=2) + '\n')
print(out)
