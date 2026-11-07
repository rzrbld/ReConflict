/* Screen fit: see screen_fit.h.
 *
 * The D3D8 backbuffer is the game's fixed size (640x480 by default) while
 * handheld panels differ (480x320, 720x720, 1280x720, ...). Drawn straight
 * into the default framebuffer it filled a corner of a larger panel or was
 * cut off on a smaller one. When the screen is another size, the backbuffer
 * becomes an offscreen colour + depth/stencil target of the game's size and
 * Present blits it into the largest centred rectangle with the game's aspect.
 *
 * GL use is limited to functions the renderer already uses (the render-thread
 * build wraps every GL call); glBlitFramebuffer comes through SDL. Present runs
 * on the GL thread, from the FPS overlay hook. DS_SCREEN_FIT=0 disables it;
 * DS_SCREEN_FIT=force uses it at equal size too (a device test of the path). */
#include "screen_fit.h"
#include <SDL_opengles2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#endif
#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8 0x88F0
#endif
#ifndef GL_DEPTH_STENCIL_ATTACHMENT
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#endif
typedef void (*BlitFn)(GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum);
static BlitFn blit;
static GLuint fbo,color,depth;
static int fit_w,fit_h;

int ds_screen_fit_init(SDL_Window *window,int gw,int gh){
    const char *mode=getenv("DS_SCREEN_FIT");   /* 0: off; force: also at equal size (tests) */
    if(mode&&!strcmp(mode,"0"))return 0;
    int force=mode&&!strcmp(mode,"force");
    int dw=0,dh=0;SDL_GL_GetDrawableSize(window,&dw,&dh);
    if(dw<=0||dh<=0||(dw==gw&&dh==gh&&!force))return 0;
    blit=(BlitFn)SDL_GL_GetProcAddress("glBlitFramebuffer");
    if(!blit){fprintf(stderr,"[screen-fit] glBlitFramebuffer unavailable; drawing at %dx%d\n",gw,gh);return 0;}
    glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,gw,gh,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D,0);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,gw,gh);
    glBindRenderbuffer(GL_RENDERBUFFER,0);
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depth);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){
        fprintf(stderr,"[screen-fit] %dx%d target incomplete; drawing at %dx%d\n",gw,gh,gw,gh);
        ds_screen_fit_close();return 0;
    }
    fit_w=gw;fit_h=gh;   /* left bound: the game's backbuffer is now the target */
    fprintf(stderr,"[screen-fit] game %dx%d scaled to fit the %dx%d screen\n",gw,gh,dw,dh);
    return 0;
}
unsigned ds_screen_fit_target(void){return fbo;}
void ds_screen_fit_present(SDL_Window *window){
    if(!fbo||!window)return;
    int dw=0,dh=0;SDL_GL_GetDrawableSize(window,&dw,&dh);
    if(dw<=0||dh<=0)return;
    GLint previous=0;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previous);
    /* largest centred rectangle with the game's aspect */
    int x0=0,y0=0,x1=dw,y1=dh;
    if((long long)dw*fit_h>(long long)dh*fit_w){int w=(int)((long long)dh*fit_w/fit_h);x0=(dw-w)/2;x1=x0+w;}
    else if((long long)dw*fit_h<(long long)dh*fit_w){int h=(int)((long long)dw*fit_h/fit_w);y0=(dh-h)/2;y1=y0+h;}
    /* Present invalidates the renderer's cached raster state afterwards, and
     * every game clear sets its own colour: no state to restore but the target. */
    glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    if(x0||y0){glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);}
    glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);
    blit(0,0,fit_w,fit_h,x0,y0,x1,y1,GL_COLOR_BUFFER_BIT,GL_LINEAR);
    /* the game resumes in its target (the overlay binds the screen itself and
     * restores this binding afterwards) */
    glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)previous);
}
void ds_screen_fit_close(void){
    if(fbo)glDeleteFramebuffers(1,&fbo);
    if(color)glDeleteTextures(1,&color);
    if(depth)glDeleteRenderbuffers(1,&depth);
    fbo=color=depth=0;fit_w=fit_h=0;
}
