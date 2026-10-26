#!/usr/bin/env python3
"""Freeze the normal build and make bounded experimental source edits only."""
from pathlib import Path
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[1]
stage = root / 'build/render-thread-game'
snapshot = stage / 'snapshot'
manifest = stage / 'inputs.json'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
sources = ['d3d8.c', 'd3d8_renderer.c', 'fps_overlay.c', 'game_main.c']
if not manifest.exists():
    if snapshot.exists():
        raise SystemExit('Incomplete snapshot exists; inspect before replacing it')
    game = root / 'build/game/desert-storm.aarch64'
    expected = (root / 'build/game/SHA256SUMS').read_text().split()[0]
    if sha(game) != expected:
        raise SystemExit('Normal executable does not match its manifest')
    inputs = [root / 'runtime' / name for name in sources]
    for pattern in ['runtime/*.h', 'probe/*.h',
                    'third_party/pcrecomp/runtime/win32hle/*.h',
                    'third_party/pcrecomp/runtime/recomp32/*.h',
                    'build/game/obj/*.o', 'build/translated/*/lifted.aarch64.a',
                    'build/target-libs/*']:
        inputs += [p for p in root.glob(pattern) if p.is_file()]
    inputs = sorted(set(inputs))
    original = {p: sha(p) for p in inputs}
    records = []
    for p, digest in original.items():
        relative = p.relative_to(root)
        target = snapshot / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(p, target, follow_symlinks=True)
        if sha(target) != digest:
            raise SystemExit(f'Snapshot mismatch: {relative}')
        records.append({'path': str(relative), 'sha256': digest})
    if any(sha(p) != digest for p, digest in original.items()) or sha(game) != expected:
        raise SystemExit('Normal build changed during snapshot; inspect before retrying')
    manifest.write_text(json.dumps({'normal_game_sha256': expected, 'files': records}, indent=2)+'\n')
data = json.loads(manifest.read_text())
for item in data['files']:
    if sha(snapshot / item['path']) != item['sha256']:
        raise SystemExit('Frozen input changed: ' + item['path'])

output = stage / 'src'
output.mkdir(exist_ok=True)
def replace(s, old, new):
    if s.count(old) != 1:
        raise SystemExit('Experimental hook anchor mismatch: ' + old[:100])
    return s.replace(old, new)

s = (snapshot / 'runtime/d3d8.c').read_text()
s = replace(s, '#include "win32hle.h"', '#include "win32hle.h"\n#include "render_thread_proxy.h"')
helpers = '''
/* Experimental only: producer owns guest/HLE state; worker owns native GL. */
static void thread_check(int result,const char *where){
    if(result){const DsRtError *e=ds_rt_error();
        if(!e)hle_fatal("render worker %s failed without an error record",where);
        hle_fatal("render worker %s: api=%s GL=%#x guest=%#x frame=%u sequence=%llu %s",
            where,e->api,e->code,e->guest,e->frame,(unsigned long long)e->sequence,e->message);}
}
extern void ds_fps_overlay_draw_dimensions(int width,int height);
static void overlay_job(void *argument){
    const int *size=argument;ds_fps_overlay_draw_dimensions(size[0],size[1]);
}
static void thread_overlay(SDL_Window *w){
    if(!ds_rt_active()){ds_fps_overlay_draw(w);return;}
    int size[2];SDL_GL_GetDrawableSize(w,&size[0],&size[1]);
    /* Blocking RPC protects overlay globals from the producer's presented hook. */
    thread_check(ds_rt_call(overlay_job,size),"overlay fence");
}
'''
s = replace(s, 'static void present(void){', helpers + '\nstatic void present(void){')
s = replace(s, 'ds_input_pump();ds_fps_overlay_draw(window);',
            'ds_input_pump();ds_rt_origin(g_cur_func,frames+1);thread_overlay(window);')
s = replace(s, 'SDL_GL_SwapWindow(window);diagnostic_poll();',
            'SDL_GL_SwapWindow(window);thread_check(ds_rt_sync(),"Present");diagnostic_poll();')
s = replace(s, 'memcpy(hle_ptr(params),presentation,sizeof presentation);return 0;',
            'memcpy(hle_ptr(params),presentation,sizeof presentation);return ds_rt_start(window,context);')
s = replace(s, 'device_obj=0;ds_fps_overlay_close();',
            'device_obj=0;if(ds_rt_stop())fprintf(stderr,"[render-thread] shutdown error: %s\\n",ds_rt_error()->message);ds_fps_overlay_close();')
s = replace(s, 'int tracing=trace_render();uint64_t begin=tracing?SDL_GetPerformanceCounter():0;',
            'ds_rt_origin(g_cur_func,frames+1);int tracing=trace_render();uint64_t begin=tracing?SDL_GetPerformanceCounter():0;')
s = replace(s, 'static RenderCounters render_frame;', '''static RenderCounters render_frame;
static uint64_t phase_previous,phase_first,phase_last;
static FILE *phase_csv;
static void phase_present(void){
    uint64_t now=SDL_GetPerformanceCounter();double scale=1000.0/SDL_GetPerformanceFrequency();
    if(!phase_csv){const char *root=getenv("DS_ROOT");char path[PATH_MAX];
        if(root&&snprintf(path,sizeof path,"%s/render-phase.csv",root)<(int)sizeof path)phase_csv=fopen(path,"w");
        if(phase_csv)fputs("frame,draws,before_first_draw_ms,draw_span_ms,after_last_draw_ms\\n",phase_csv);}
    if(phase_csv&&phase_previous&&phase_first&&phase_last>=phase_first)
        fprintf(phase_csv,"%u,%llu,%.6f,%.6f,%.6f\\n",frames,(unsigned long long)render_frame.draws,
            (phase_first-phase_previous)*scale,(phase_last-phase_first)*scale,(now-phase_last)*scale);
    if(phase_csv&&frames%30==0)fflush(phase_csv);
    phase_previous=now;phase_first=phase_last=0;
}''')
s = replace(s, 'static void trace_draw_start(void){if(trace_render()){render_frame.draws++;',
            'static void trace_draw_start(void){if(trace_render()){if(!phase_first)phase_first=SDL_GetPerformanceCounter();render_frame.draws++;')
s = replace(s, 'if(tracing)render_frame.submit_ms+=(SDL_GetPerformanceCounter()-begin)*1000.0/SDL_GetPerformanceFrequency();',
            'if(tracing){phase_last=SDL_GetPerformanceCounter();render_frame.submit_ms+=(phase_last-begin)*1000.0/SDL_GetPerformanceFrequency();}')
s = replace(s, 'render_seen_draw|=render_frame.draws!=0;',
            'phase_present();render_seen_draw|=render_frame.draws!=0;')
s = replace(s, 'void ds_graphics_profile_flush(void){if(profile_csv)',
            'void ds_graphics_profile_flush(void){if(phase_csv)fflush(phase_csv);if(profile_csv)')
(output / 'd3d8.c').write_text(s)

s = (snapshot / 'runtime/d3d8_renderer.c').read_text()
s = '#include "render_thread_proxy.h"\n' + s
s = replace(s, 'GLenum e=glGetError();if(e){d8_renderer_invalidate_raster(r);',
            'GLenum e=ds_rt_defer_draw_error();if(e){d8_renderer_invalidate_raster(r);')
(output / 'd3d8_renderer.c').write_text(s)

s = (snapshot / 'runtime/fps_overlay.c').read_text()
old = '''void ds_fps_overlay_draw(SDL_Window *window) {
    if(!overlay_enabled()||failed||!window)return;
    int width,height;SDL_GL_GetDrawableSize(window,&width,&height);if(width<1||height<1)return;'''
new = '''void ds_fps_overlay_draw_dimensions(int width,int height) {
    if(!overlay_enabled()||failed||width<1||height<1)return;'''
s = replace(s, old, new)
s += '''
void ds_fps_overlay_draw(SDL_Window *window){
    if(!window)return;int w,h;SDL_GL_GetDrawableSize(window,&w,&h);
    ds_fps_overlay_draw_dimensions(w,h);
}
'''
(output / 'fps_overlay.c').write_text(s)

s = (snapshot / 'runtime/game_main.c').read_text()
s = replace(s, '#include "win32hle.h"', '#include "win32hle.h"\n#include "render_thread_proxy.h"')
s = replace(s, 'static void game_exit(int code){(void)code;ds_profiler_stop();}',
            'static void game_exit(int code){(void)code;if(ds_rt_stop())fprintf(stderr,"[render-thread] exit error: %s\\n",ds_rt_error()->message);ds_profiler_stop();}')
(output / 'game_main.c').write_text(s)
print('Experimental source staged from normal ELF ' + data['normal_game_sha256'])
