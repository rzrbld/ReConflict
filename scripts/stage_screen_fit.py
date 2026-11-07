#!/usr/bin/env python3
"""Scale the game's backbuffer to any screen (render-thread release facade).

The release facade (build/frame-limit/d3d8.c) draws the D3D8 backbuffer
straight into the default framebuffer at the game's size (640x480), so on
other panels it filled a corner or was cut off. This stage gives it the
runtime/screen_fit.c behaviour: when the screen is another size, the
backbuffer is an offscreen target of the game's size and Present blits it
into the largest centred rectangle with the game's aspect (black bars).
The blit runs where the FPS overlay is drawn: on the GL worker when the
render thread is active, otherwise on the game thread. DS_SCREEN_FIT=0
disables it; DS_SCREEN_FIT=force uses it at equal size too (device test).

Output: build/screen-fit/ (the facade plus the files it includes).
Fail-closed like the other stages: exact input hash, one match per anchor.
"""
from pathlib import Path
import hashlib, json, shutil

ROOT = Path(__file__).resolve().parents[1]
base = ROOT / 'build/frame-limit'
out = ROOT / 'build/screen-fit'
out.mkdir(exist_ok=True)
s = (base / 'd3d8.c').read_text()
assert hashlib.sha256(s.encode()).hexdigest() == 'c4f9895f70f7759a4ecde04b136fcafe680915d0ad64ca62acc3b29b6896a655'

def replace(old, new):
    global s
    assert s.count(old) == 1, ('anchor', old[:100])
    s = s.replace(old, new)

FIT = r'''
/* ---- screen fit (scripts/stage_screen_fit.py; see runtime/screen_fit.c) ---- */
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#endif
#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8 0x88F0
#endif
#ifndef GL_DEPTH_STENCIL_ATTACHMENT
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#endif
typedef void (*FitBlitFn)(GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum);
static FitBlitFn fit_blit;
static GLuint fit_fbo,fit_color,fit_depth;
static int fit_w,fit_h;
static void fit_close(void){
    if(fit_fbo)glDeleteFramebuffers(1,&fit_fbo);
    if(fit_color)glDeleteTextures(1,&fit_color);
    if(fit_depth)glDeleteRenderbuffers(1,&fit_depth);
    fit_fbo=fit_color=fit_depth=0;fit_w=fit_h=0;
}
/* After context creation, before the render worker starts. */
static void fit_init(SDL_Window *window,int gw,int gh){
    const char *mode=getenv("DS_SCREEN_FIT");
    if(mode&&!strcmp(mode,"0"))return;
    int force=mode&&!strcmp(mode,"force");
    int dw=0,dh=0;SDL_GL_GetDrawableSize(window,&dw,&dh);
    if(dw<=0||dh<=0||(dw==gw&&dh==gh&&!force))return;
    fit_blit=(FitBlitFn)SDL_GL_GetProcAddress("glBlitFramebuffer");
    if(!fit_blit){fprintf(stderr,"[screen-fit] glBlitFramebuffer unavailable; drawing at %dx%d\n",gw,gh);return;}
    glGenTextures(1,&fit_color);glBindTexture(GL_TEXTURE_2D,fit_color);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,gw,gh,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D,0);
    glGenRenderbuffers(1,&fit_depth);glBindRenderbuffer(GL_RENDERBUFFER,fit_depth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,gw,gh);glBindRenderbuffer(GL_RENDERBUFFER,0);
    glGenFramebuffers(1,&fit_fbo);glBindFramebuffer(GL_FRAMEBUFFER,fit_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,fit_color,0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,fit_depth);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){
        fprintf(stderr,"[screen-fit] %dx%d target incomplete; drawing at %dx%d\n",gw,gh,gw,gh);
        glBindFramebuffer(GL_FRAMEBUFFER,0);fit_close();return;
    }
    fit_w=gw;fit_h=gh;   /* left bound: the backbuffer is now this target */
    fprintf(stderr,"[screen-fit] game %dx%d scaled to fit the %dx%d screen\n",gw,gh,dw,dh);
}
/* On the GL thread before the overlay and swap. Present invalidates the
 * renderer's cached raster state afterwards and every game clear sets its own
 * colour, so only the framebuffer binding is restored. */
static void fit_present(int dw,int dh){
    if(!fit_fbo||dw<=0||dh<=0)return;
    GLint previous=0;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previous);
    int x0=0,y0=0,x1=dw,y1=dh;
    if((long long)dw*fit_h>(long long)dh*fit_w){int w=(int)((long long)dh*fit_w/fit_h);x0=(dw-w)/2;x1=x0+w;}
    else if((long long)dw*fit_h<(long long)dh*fit_w){int h=(int)((long long)dw*fit_h/fit_w);y0=(dh-h)/2;y1=y0+h;}
    glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    if(x0||y0){glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);}
    glBindFramebuffer(GL_READ_FRAMEBUFFER,fit_fbo);
    fit_blit(0,0,fit_w,fit_h,x0,y0,x1,y1,GL_COLOR_BUFFER_BIT,GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)previous);
}
/* ---- end screen fit ---- */
'''
replace('#include "dispatch_cache.h"\n', '#include "dispatch_cache.h"\n' + FIT)
replace('if(c->backbuffer){glBindFramebuffer(GL_FRAMEBUFFER,0);}',
        'if(c->backbuffer){glBindFramebuffer(GL_FRAMEBUFFER,fit_fbo);}')
replace('if(surface->backbuffer)glBindFramebuffer(GL_FRAMEBUFFER,0);',
        'if(surface->backbuffer)glBindFramebuffer(GL_FRAMEBUFFER,fit_fbo);')
replace('SDL_ShowCursor(SDL_DISABLE);', 'SDL_ShowCursor(SDL_DISABLE);fit_init(window,(int)w,(int)h);')
replace('ds_fps_overlay_close();', 'ds_fps_overlay_close();fit_close();')
replace('ds_fps_overlay_draw_dimensions(job->size[0],job->size[1]);',
        'fit_present(job->size[0],job->size[1]);ds_fps_overlay_draw_dimensions(job->size[0],job->size[1]);')
replace('if(!ds_rt_active()){ds_fps_overlay_draw(w);return;}',
        'if(!ds_rt_active()){int fdw=0,fdh=0;SDL_GL_GetDrawableSize(w,&fdw,&fdh);fit_present(fdw,fdh);ds_fps_overlay_draw(w);return;}')
(out / 'd3d8.c').write_text(s)
for p in base.iterdir():
    if p.suffix in {'.c', '.h'} and p.name != 'd3d8.c':
        shutil.copyfile(p, out / p.name)
(out / 'manifest.json').write_text(json.dumps({p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                               for p in sorted(out.iterdir()) if p.suffix in {'.h', '.c'}}, indent=2) + '\n')
print(out)
