#define _POSIX_C_SOURCE 200809L
#include <SDL.h>
#include <GLES2/gl2.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "d3d8_renderer.h"
#include "render_thread_proxy.h"
extern uint64_t ds_glow_shader_draws(void);
#define W 128
#define H 128
static unsigned char pixels[2][W*H*4];
typedef struct {float p[3];uint32_t c;float uv[2];} Vertex;
typedef struct {float p[4];uint32_t c;float uv[2];} Rhw;
static void float_bits(uint32_t *to,float value){memcpy(to,&value,4);}
static void state(D8State *s,unsigned w,unsigned h){
 d8_state_defaults(s,w,h);s->fvf=0x142;s->rs[7]=0;s->rs[14]=0;s->rs[22]=1;s->rs[137]=0;
 s->rs[27]=1;s->rs[19]=5;s->rs[20]=6;s->rs[15]=s->rs[28]=s->rs[29]=0;
 s->tss[0][1]=s->tss[0][4]=4;s->tss[0][2]=s->tss[0][5]=2;s->tss[0][3]=s->tss[0][6]=0;
 s->tss[1][1]=1;s->tss[0][4]=2;
}
int main(void){
 assert(!SDL_Init(SDL_INIT_VIDEO));
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
 SDL_Window *window=SDL_CreateWindow("glow shader parity",0,0,640,480,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
 assert(window);SDL_GLContext context=SDL_GL_CreateContext(window);assert(context);
 printf("GL=%s renderer=%s\n",glGetString(GL_VERSION),glGetString(GL_RENDERER));
 unsigned global_max=0;int failed=0;
 for(int gpu=0;gpu<2;gpu++){
  setenv("DS_GPU_VERTEX",gpu?"1":"0",1);
  D8Renderer renderer[2];setenv("DS_FAST_PARTICLES","0",1);setenv("DS_FAST_FOG","0",1);setenv("DS_FAST_GLOW","0",1);assert(!d8_renderer_init(renderer));
  setenv("DS_FAST_GLOW","1",1);assert(!d8_renderer_init(renderer+1));
  setenv("DS_RENDER_THREAD","1",1);assert(!ds_rt_start(window,context));assert(ds_rt_active());
  GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
  const unsigned char image[]={255,32,64,0,64,255,32,85,32,64,255,170,240,210,180,255};
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,image);
  Vertex v[]={{{-1,-1,.5},0x80ff4030,{0,1}},{{1,-1,.5},0x5000ff60,{1,1}},{{-1,1,.5},0xc04080ff,{0,0}},{{1,1,.5},0x00ffffff,{1,0}}};
  Rhw q[]={{{-.5,H-.5,.5,1},0x80ff4030,{0,1}},{{W-.5,H-.5,.5,.8},0x5000ff60,{1,1}},{{-.5,-.5,.5,1.2},0xc04080ff,{0,0}},{{W-.5,-.5,.5,1},0x00ffffff,{1,0}}};
  GLuint buffers[2];glGenBuffers(2,buffers);
  unsigned char vertex_data[32+sizeof v];memset(vertex_data,0,32);memcpy(vertex_data+32,v,sizeof v);
  const uint16_t index_data[]={0,0,0,1,2,3};
  glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);glBufferData(GL_ARRAY_BUFFER,sizeof vertex_data,vertex_data,GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,buffers[1]);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof index_data,index_data,GL_STATIC_DRAW);
  D8NativeBuffers native={.vertex=buffers[0],.index=buffers[1],.vertex_offset=32,.index_offset=4,.vertex_bytes=sizeof vertex_data,.index_bytes=sizeof index_data,.default_framebuffer=1};
  for(int test=0;test<19;test++){
   D8State s;state(&s,W,H);GLuint textures[]={texture,0};int rhw=test==1||test==13;
   if(rhw)s.fvf=0x144;
   if(test==2)s.texture_flipy[0]=1;
   if(test==3){s.tss[0][24]=2;s.transform[16][0]=.6f;s.transform[16][5]=.7f;s.transform[16][12]=.2f;}
   if(test==4){s.rs[28]=1;s.rs[35]=3;float_bits(&s.rs[36],0);float_bits(&s.rs[37],2);s.rs[34]=0xff307060;}
   if(test==5){s.rs[15]=1;s.rs[25]=5;s.rs[24]=80;}
   if(test==6)s.rs[29]=1;
   if(test==7)s.tss[0][1]=2;
   if(test==8)s.tss[0][4]=4;
   if(test==9)s.tss[0][6]=32;
   if(test==10){s.tss[1][1]=4;textures[1]=texture;}
   if(test==11)textures[0]=0;
   if(test==12)s.rs[20]=2;
   if(test==13)s.rs[27]=0;
   if(test==14){s.tss[0][16]=s.tss[0][17]=2;}
   if(test==15){s.tss[0][13]=s.tss[0][14]=3;}
   if(test==16){s.viewport[0]=7;s.viewport[1]=9;s.viewport[2]=83;s.viewport[3]=77;}
   if(test==17){s.tss[0][3]=1;} /* Equivalent arithmetic, deliberately outside strict key. */
   int eligible=!(test>=4&&test<=11)&&test!=17;
   uint64_t before=ds_glow_shader_draws();
   for(int mode=0;mode<2;mode++){
    glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);glClearColor(.1f,.2f,.3f,.4f);glClear(GL_COLOR_BUFFER_BIT);
    d8_renderer_invalidate_raster(renderer+mode);d8_renderer_invalidate_attributes(renderer+mode);
    for(int layer=0;layer<3;layer++){
     if(test==18)assert(!d8_renderer_draw_native(renderer+mode,&s,H,5,2,v,sizeof v[0],4,index_data+2,2,0,textures,&native));
     else assert(!d8_renderer_draw(renderer+mode,&s,H,5,2,rhw?(void*)q:(void*)v,rhw?sizeof(q[0]):sizeof(v[0]),4,NULL,0,0,textures));
    }
    glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,pixels[mode]);assert(!glGetError());
   }
   uint64_t used=ds_glow_shader_draws()-before;assert(used==(eligible?3u:0u));
   unsigned max=0,bad=0,painted=0;
   for(size_t i=0;i<sizeof pixels[0];i++){
    unsigned d=(unsigned)abs((int)pixels[0][i]-(int)pixels[1][i]);if(d>max)max=d;if(d>2)bad++;
    if(i%4==0&&pixels[0][i]>40)painted++;
   }
   /* Stage 1's default alpha operation in case 10 produces transparent
    * fragments; parity and strict fallback are the assertions for that case. */
   if(max>global_max)global_max=max;if(bad||(!painted&&test!=10))failed=1;
   printf("gpu=%d case=%d specialized_draws=%llu maxdiff=%u channels_gt2=%u painted=%u\n",gpu,test,(unsigned long long)used,max,bad,painted);
  }
  if(gpu&&getenv("DS_GLOW_BENCH")){
   D8State s;state(&s,640,480);GLuint textures[]={texture,0};
   for(int mode=0;mode<2;mode++){
    double begin=0;
    for(int frame=0;frame<7;frame++){
     if(frame==2)begin=(double)SDL_GetPerformanceCounter()/SDL_GetPerformanceFrequency();
     glClear(GL_COLOR_BUFFER_BIT);
     for(int layer=0;layer<100;layer++)assert(!d8_renderer_draw(renderer+mode,&s,480,5,2,v,sizeof v[0],4,NULL,0,0,textures));
     glFinish();
    }
    double end=(double)SDL_GetPerformanceCounter()/SDL_GetPerformanceFrequency();
    printf("overdraw_kernel mode=%s layers=100 resolution=640x480 frames=5 mean_ms=%.6f (not gameplay FPS)\n",mode?"specialized":"generic",(end-begin)*1000/5);
   }
  }
  glDeleteBuffers(2,buffers);glDeleteTextures(1,&texture);assert(!ds_rt_stop());d8_renderer_close(renderer+1);d8_renderer_close(renderer);
 }
 printf("glow_shader_parity=%s maxdiff=%u\n",failed?"FAIL":"PASS",global_max);
 SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return failed;
}
