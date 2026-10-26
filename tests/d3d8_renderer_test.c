#include <SDL.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <string.h>
#include "d3d8_renderer.h"
int main(void) {
    if (SDL_Init(SDL_INIT_VIDEO)) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    SDL_Window *w=SDL_CreateWindow("D3D8 renderer test",0,0,640,480,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    if(!w){fprintf(stderr,"window: %s\n",SDL_GetError());return 1;}
    SDL_GLContext gl=SDL_GL_CreateContext(w);
    if(!gl){fprintf(stderr,"context: %s\n",SDL_GetError());return 1;}
    D8Renderer r;D8State state;
    if(d8_renderer_init(&r))return 1;
    d8_state_defaults(&state,640,480);state.rs[7]=0;state.rs[22]=1;state.fvf=0x44;
    struct {float xyzw[4];uint32_t argb;} triangle[]={
        {{-.5f,479.5f,.5f,1},0xffff0000},{{639.5f,479.5f,.5f,1},0xffff0000},{{319.5f,-.5f,.5f,1},0xffff0000}};
    glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);
    GLuint textures[2]={0,0};
    if(d8_renderer_draw(&r,&state,480,4,1,triangle,sizeof triangle[0],3,NULL,0,0,textures))return 1;
    unsigned char center[4]={0},corner[4]={0};
    glReadPixels(320,240,1,1,GL_RGBA,GL_UNSIGNED_BYTE,center);glReadPixels(1,478,1,1,GL_RGBA,GL_UNSIGNED_BYTE,corner);
    int ok=center[0]>240&&center[1]<15&&center[2]<15&&corner[0]<15&&corner[1]<15&&corner[2]>240;
    /* An alpha test must reject geometry rather than painting opaque black. */
    state.rs[15]=1;state.rs[25]=5;state.rs[24]=200;state.tss[0][5]=0;
    for(int i=0;i<3;i++)triangle[i].argb=0x80ff0000u;
    glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);
    if(d8_renderer_draw(&r,&state,480,4,1,triangle,sizeof triangle[0],3,NULL,0,0,textures))return 1;
    glReadPixels(320,240,1,1,GL_RGBA,GL_UNSIGNED_BYTE,center);
    ok=ok&&center[0]<15&&center[2]>240;
    /* Source-alpha blending must preserve the destination's blue channel. */
    state.rs[15]=0;state.rs[27]=1;state.rs[19]=5;state.rs[20]=6;
    if(d8_renderer_draw(&r,&state,480,4,1,triangle,sizeof triangle[0],3,NULL,0,0,textures))return 1;
    glReadPixels(320,240,1,1,GL_RGBA,GL_UNSIGNED_BYTE,center);
    ok=ok&&center[0]>120&&center[0]<136&&center[1]<15&&center[2]>120&&center[2]<136;
    printf("renderer=%s center=%u,%u,%u corner=%u,%u,%u result=%s\n",glGetString(GL_RENDERER),center[0],center[1],center[2],corner[0],corner[1],corner[2],ok?"PASS":"FAIL");
    d8_renderer_close(&r);SDL_GL_DeleteContext(gl);SDL_DestroyWindow(w);SDL_Quit();return ok?0:1;
}
