/* D3D8 texture cascade termination, tested independently of CPU/GPU parity. */
#include <SDL.h>
#include <GLES2/gl2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "d3d8_renderer.h"
static const struct {float p[4];uint32_t color;} triangle[]={
    {{-.5f,63.5f,.5f,1},0xffff0000},{{63.5f,63.5f,.5f,1},0xffff0000},{{31.5f,-.5f,.5f,1},0xffff0000}};
static int check(D8Renderer *r,D8State *s,const char *name,int red){
    GLuint textures[2]={0,0};unsigned char pixel[4]={0};
    glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);
    if(d8_renderer_draw(r,s,64,4,1,triangle,sizeof triangle[0],3,NULL,0,0,textures)){
        fprintf(stderr,"%s rejected: %s\n",name,d8_renderer_error());return 0;
    }
    glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    int ok=red?pixel[0]>240&&pixel[1]<15&&pixel[2]<15:pixel[0]<15&&pixel[1]>240&&pixel[2]<15;
    printf("%s pixel=%u,%u,%u result=%s\n",name,pixel[0],pixel[1],pixel[2],ok?"PASS":"FAIL");return ok;
}
int main(void){
    if(SDL_Init(SDL_INIT_VIDEO))return 2;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_Window *w=SDL_CreateWindow("cascade test",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);if(!w)return 2;
    SDL_GLContext context=SDL_GL_CreateContext(w);if(!context)return 2;
    D8Renderer r;D8State s;if(d8_renderer_init(&r))return 2;d8_state_defaults(&s,64,64);s.fvf=0x44;s.rs[7]=0;s.rs[22]=1;s.rs[60]=0xff00ff00;
    s.tss[0][1]=1;s.tss[1][1]=2;s.tss[1][2]=3;int ok=check(&r,&s,"stage0_disabled_stage1_green_ignored",1);
    s.tss[1][1]=99;s.tss[1][4]=99;s.tss[1][11]=0x00010009;s.tss[1][24]=256;
    ok=check(&r,&s,"disabled_chain_ignores_unsupported_stage1",1)&&ok;
    s.tss[0][11]=0x00010009;s.tss[0][24]=256;s.tss[0][4]=99;
    ok=check(&r,&s,"disabled_stage0_ignores_coordinate_and_alpha_state",1)&&ok;
    s.tss[0][1]=2;s.tss[0][2]=0;s.tss[0][4]=2;s.tss[0][11]=0;s.tss[0][24]=0;s.tss[1][1]=1;
    ok=check(&r,&s,"stage1_disabled_ignores_its_unsupported_state",1)&&ok;
    s.tss[1][1]=99;GLuint textures[2]={0};
    int rejected=d8_renderer_draw(&r,&s,64,4,1,triangle,sizeof triangle[0],3,NULL,0,0,textures)!=0&&strstr(d8_renderer_error(),"unsupported TSS stage=1");
    printf("active_unsupported_stage1_rejected=%s\n",rejected?"PASS":"FAIL");ok=ok&&rejected;
    s.tss[1][1]=2;s.tss[1][2]=3;s.tss[1][4]=2;s.tss[1][11]=0;s.tss[1][24]=0;
    ok=check(&r,&s,"enabled_stage1_still_modifies_output",0)&&ok;
    /* Exact scope-state regression: missing texture, COLOROP=MODULATE,
     * ALPHAOP=SELECTARG1(TEXTURE), vertex ARGB=0x40fca90b. D3D terminates
     * this stage, retaining 25%-alpha orange instead of making it opaque. */
    struct {float p[4];uint32_t color;float uv[2];} scope[3];
    for(int i=0;i<3;i++){memcpy(scope[i].p,triangle[i].p,16);scope[i].color=0x40fca90b;scope[i].uv[0]=scope[i].uv[1]=0;}
    d8_state_defaults(&s,64,64);s.fvf=0x144;s.rs[7]=0;s.rs[22]=1;s.rs[27]=1;s.rs[19]=5;s.rs[20]=6;
    /* A stale higher stage must be bypassed by null-texture termination too. */
    glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);
    int rc=d8_renderer_draw(&r,&s,64,4,1,scope,sizeof scope[0],3,NULL,0,0,textures);unsigned char pixel[4]={0};glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    int scope_ok=!rc&&pixel[0]>=62&&pixel[0]<=64&&pixel[1]>=41&&pixel[1]<=43&&pixel[2]>=193&&pixel[2]<=195;
    printf("null_texture_scope_tint pixel=%u,%u,%u expected=63,42,194 result=%s\n",pixel[0],pixel[1],pixel[2],scope_ok?"PASS":"FAIL");ok=ok&&scope_ok;
    s.tss[1][1]=2;s.tss[1][2]=3;s.rs[60]=0xff00ff00;glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);
    rc=d8_renderer_draw(&r,&s,64,4,1,scope,sizeof scope[0],3,NULL,0,0,textures);glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    scope_ok=!rc&&pixel[0]>=62&&pixel[0]<=64&&pixel[1]>=41&&pixel[1]<=43&&pixel[2]>=193&&pixel[2]<=195;
    printf("null_texture_terminates_later_stage pixel=%u,%u,%u result=%s\n",pixel[0],pixel[1],pixel[2],scope_ok?"PASS":"FAIL");ok=ok&&scope_ok;

    printf("backend=%s overall=%s\n",r.gpu_program?"GPU":"CPU",ok?"PASS":"FAIL");
    d8_renderer_close(&r);SDL_GL_DeleteContext(context);SDL_DestroyWindow(w);SDL_Quit();return ok?0:1;
}
