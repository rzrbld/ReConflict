#!/usr/bin/env python3
"""Add loading-only retention hooks to checked, locally translated input."""
from pathlib import Path
import hashlib,json,shutil,subprocess
root=Path(__file__).resolve().parents[1];out=root/'build/loading-background';out.mkdir(exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
source=root/'build/translated/desertstorm/functions_0003.c'
s=source.read_text();assert sha(s.encode())=='cd206c5fdac90cd32f64ccdd00a954f7fbd3e79616332cf7db3b1af24e8c2be0'
archive=root/'build/render-thread-game/snapshot/build/translated/desertstorm/lifted.aarch64.a'
old_obj=subprocess.check_output(['ar','p',str(archive),'functions_0003.aarch64.o'])
assert sha(old_obj)=='5b222140856256dab973db741a215cc05e3c69cc79b4a9b94067e5d9b8be3299'
def replace(s,a,b):
    assert s.count(a)==1,a
    return s.replace(a,b)
s=replace(s,'#include "functions.h"','#include "functions.h"\nvoid ds_game_loading_capture(void);\nvoid ds_game_loading_restore(void);')
for address,target,hook in [('0040A8A3','00541630','capture'),('0040ABE7','00546B70','restore'),('0040ABEC','0040B0B0','capture'),('0040ACCF','00546B70','restore'),('0040B073','00546BB0','capture')]:
    anchor=f'RECOMP_CALL(recomp_desertstorm_{target}); /* 0x{address}: call 0x{int(target,16):x} */\n    RECOMP_FLAGS_IN();'
    condition='if(esi==1) ' if address=='0040ABEC' else ''
    s=replace(s,anchor,anchor+f'\n    {condition}ds_game_loading_{hook}(); /* loading-only GLES backbuffer retention */')
(out/'functions_0003.c').write_text(s);shutil.copy(source.parent/'functions.h',out/'functions.h')
facade=root/'build/present-timing/d3d8.c';s=facade.read_text()
assert sha(s.encode())=='5e9bf15f21b6e24ad3aabac671edabada7a3e92368f3cf029b16c8fa1d2f329a'
s='#include "loading_background.c"\n'+s
s=replace(s,'device_obj=0;if(ds_rt_stop())','device_obj=0;if(ds_rt_stop())') # verify lifecycle anchor
s=replace(s,'ds_fps_overlay_close();d8_renderer_close(&renderer);','ds_loading_background_close(&loading_background);ds_fps_overlay_close();d8_renderer_close(&renderer);')
s=replace(s,'static uint32_t vtables[7],','static DsLoadingBackground loading_background;\nstatic uint32_t vtables[7],')
s+='''
typedef struct {int width,height,capture,result;} LoadingJob;
static void loading_background_job(void *arg) {
    LoadingJob *j=arg;
    j->result=j->capture?ds_loading_background_capture(&loading_background,j->width,j->height):ds_loading_background_restore(&loading_background,j->width,j->height);
}
static void game_loading_background(int capture) {
    const char *setting=getenv("DS_LOADING_BACKGROUND");
    if((setting&&!strcmp(setting,"0"))||!window||render_target!=backbuffer_obj)return;
    LoadingJob job={.capture=capture};SDL_GL_GetDrawableSize(window,&job.width,&job.height);
    thread_check(ds_rt_call(loading_background_job,&job),"loading background");
    d8_renderer_invalidate_raster(&renderer);d8_renderer_invalidate_attributes(&renderer);
    if(capture||job.result)fprintf(stderr,"[loading-background] %s frame=%u size=%dx%d result=%d\\n",capture?"capture":"restore",frames+1,job.width,job.height,job.result);
}
void ds_game_loading_capture(void){game_loading_background(1);}
void ds_game_loading_restore(void){game_loading_background(0);}
'''
(out/'d3d8.c').write_text(s)
for name in ['loading_background.c','loading_background.h']:shutil.copy(root/'runtime'/name,out/name)
for name in ['present_timing.c','present_timing.h']:shutil.copy(facade.parent/name,out/name)
report={'source_sha256':sha(source.read_bytes()),'original_object_sha256':sha(old_obj),'facade_sha256':sha(facade.read_bytes()),'hooks':['0040A8A3 after background draw','0040ABE7 restore initial image before briefing','0040ABEC capture second composed title frame','0040ACCF after progress BeginScene','0040B073 capture black mission briefing before Present']}
(out/'manifest.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
