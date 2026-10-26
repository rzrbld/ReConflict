#define _POSIX_C_SOURCE 200809L
#include <SDL.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "d3d8_renderer.h"
#ifdef DS_TEST_BUFFER_CALLS
static unsigned data_calls,subdata_calls;
void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
void __real_glBufferSubData(GLenum,GLintptr,GLsizeiptr,const void*);
void __wrap_glBufferData(GLenum t,GLsizeiptr n,const void *p,GLenum u){data_calls++;__real_glBufferData(t,n,p,u);}
void __wrap_glBufferSubData(GLenum t,GLintptr o,GLsizeiptr n,const void *p){subdata_calls++;__real_glBufferSubData(t,o,n,p);}
#endif
#define SIDE 64
static unsigned char pixels[2][SIDE*SIDE*4];
typedef struct {float p[3],n[3];uint32_t c,spec;float uv[2];} V;
static int compare(const char *name,D8Renderer r[2],D8State *s,const void *data,unsigned vcount,const void *indices,unsigned isize,unsigned base,GLuint textures[2],D8NativeBuffers *native,int expected_data_calls,int expect_paint){
 for(int k=0;k<2;k++){
  glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);d8_renderer_take_upload_stats(r+k);
#ifdef DS_TEST_BUFFER_CALLS
  data_calls=subdata_calls=0;
#endif
  if(d8_renderer_draw_native(r+k,s,SIDE,4,1,data,sizeof(V),vcount,indices,isize,base,textures,native)){fprintf(stderr,"%s: %s\n",name,d8_renderer_error());return 0;}
  glReadPixels(0,0,SIDE,SIDE,GL_RGBA,GL_UNSIGNED_BYTE,pixels[k]);
  if(k==1){D8UploadStats stats=d8_renderer_take_upload_stats(r+1);
   if(stats.native_vertex_draws!=(unsigned)(native->vertex!=0)||stats.native_index_draws!=(unsigned)(indices&&native->index!=0)){fprintf(stderr,"%s native-draw counters mismatch\n",name);return 0;}
#ifdef DS_TEST_BUFFER_CALLS
   if(data_calls!=(unsigned)expected_data_calls||subdata_calls){fprintf(stderr,"%s unexpected renderer uploads data=%u subdata=%u expected=%d\n",name,data_calls,subdata_calls,expected_data_calls);return 0;}
#else
   (void)expected_data_calls;
#endif
  }
 }
 unsigned bad=0,max=0,paint=0;for(unsigned i=0;i<sizeof pixels[0];i++){unsigned d=abs((int)pixels[0][i]-pixels[1][i]);if(d>max)max=d;if(d>2)bad++;if(i%4==0&&pixels[0][i]>20)paint++;}
 int pass=bad<5&&(expect_paint?paint>100:paint==0);printf("%s maxdiff=%u channels_gt2=%u painted=%u result=%s\n",name,max,bad,paint,pass?"PASS":"FAIL");return pass;
}
int main(void){
 if(SDL_Init(SDL_INIT_VIDEO))return 2;SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
 SDL_Window *window=SDL_CreateWindow("native buffers",0,0,SIDE,SIDE,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);if(!window)return 2;SDL_GLContext context=SDL_GL_CreateContext(window);if(!context)return 2;
 D8Renderer r[2];unsetenv("DS_GPU_VERTEX");if(d8_renderer_init(r))return 2;setenv("DS_GPU_VERTEX","1",1);if(d8_renderer_init(r+1)||!r[1].gpu_program)return 2;
 D8State state;d8_state_defaults(&state,SIDE,SIDE);state.fvf=0x1d2;state.rs[7]=0;state.rs[22]=1;state.rs[137]=0;state.rs[29]=1;
 V vertices[12]={0};const V tri[]={{{-.75,-.75,.5},{0,0,-1},0xffe09060,0x00030303,{0,0}},{{.75,-.75,.5},{0,0,-1},0xff60e090,0x00030303,{1,0}},{{0,.75,.5},{0,0,-1},0xff9060e0,0x00030303,{.5,1}}};memcpy(vertices+6,tri,sizeof tri);
 unsigned char storage[16+sizeof vertices];memset(storage,0,sizeof storage);memcpy(storage+16,vertices,sizeof vertices);
 uint16_t idx16[]={0,0,0,0,4,5,6};uint32_t idx32[]={0,0,0,4,5,6};
 GLuint vb,ib16,ib32,texture;glGenBuffers(1,&vb);glBindBuffer(GL_ARRAY_BUFFER,vb);glBufferData(GL_ARRAY_BUFFER,sizeof storage,storage,GL_STATIC_DRAW);
 glGenBuffers(1,&ib16);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ib16);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof idx16,idx16,GL_STATIC_DRAW);
 glGenBuffers(1,&ib32);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ib32);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof idx32,idx32,GL_STATIC_DRAW);
 glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);unsigned char rgba[]={255,64,32,255,32,255,64,255,64,32,255,255,240,240,240,255};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);GLuint textures[]={texture,0};
 D8NativeBuffers native={vb,ib16,16,8,sizeof storage,sizeof idx16};int ok=compare("native16_min4_base2_offsets",r,&state,vertices,10,idx16+4,2,2,textures,&native,0,1);
 native.index=ib32;native.index_offset=12;native.index_bytes=sizeof idx32;ok=compare("native32_min4_base2_offsets",r,&state,vertices,10,idx32+3,4,2,textures,&native,0,1)&&ok;
 native.index=0;ok=compare("native_VB_staged_IB",r,&state,vertices,10,idx16+4,2,2,textures,&native,1,1)&&ok;
 native.vertex=0;native.index=ib16;native.index_offset=8;native.index_bytes=sizeof idx16;ok=compare("staged_VB_native_IB",r,&state,vertices,10,idx16+4,2,2,textures,&native,1,1)&&ok;
 native.vertex=vb;native.index=0;native.vertex_offset=16+6*sizeof(V);ok=compare("native_nonindexed_vertex_offset",r,&state,vertices+6,3,NULL,0,0,textures,&native,0,1)&&ok;
 for(int i=6;i<9;i++)vertices[i].c=0xff40c0f0;glBindBuffer(GL_ARRAY_BUFFER,vb);glBufferSubData(GL_ARRAY_BUFFER,16+6*sizeof(V),3*sizeof(V),vertices+6);
 d8_renderer_note_buffer_upload(r+1,0,3*sizeof(V));D8UploadStats uploaded=d8_renderer_take_upload_stats(r+1);if(uploaded.vertex_bytes!=3*sizeof(V)||uploaded.vertex_uploads!=1)ok=0;
 native.vertex_offset=16;native.index=ib16;ok=compare("native_VB_dirty_subrange_update",r,&state,vertices,10,idx16+4,2,2,textures,&native,0,1)&&ok;
 idx16[5]=idx16[6]=4;glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ib16);glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,8,6,idx16+4);
 ok=compare("native_IB_dirty_subrange_degenerate",r,&state,vertices,10,idx16+4,2,2,textures,&native,0,0)&&ok;
 D8NativeBuffers invalid=native;invalid.vertex_bytes=1;int rc=d8_renderer_draw_native(r+1,&state,SIDE,4,1,vertices,sizeof(V),10,idx16+4,2,2,textures,&invalid);
 int bounds=rc!=0&&strstr(d8_renderer_error(),"native vertex buffer range");invalid=native;invalid.index_offset=invalid.index_bytes;
 rc=d8_renderer_draw_native(r+1,&state,SIDE,4,1,vertices,sizeof(V),10,idx16+4,2,2,textures,&invalid);bounds=bounds&&rc!=0&&strstr(d8_renderer_error(),"native index buffer range");
 rc=d8_renderer_draw_native(r,&state,SIDE,4,1,vertices,sizeof(V),10,idx16+4,2,2,textures,&invalid);bounds=bounds&&rc==0;printf("native_bounds_rejected_CPU_fallback_ignores_handles=%s\n",bounds?"PASS":"FAIL");ok=ok&&bounds;
 d8_renderer_close(r);d8_renderer_close(r+1);int ownership=glIsBuffer(vb)&&glIsBuffer(ib16)&&glIsBuffer(ib32);printf("external_buffer_ownership=%s\n",ownership?"PASS":"FAIL");ok=ok&&ownership;
 glDeleteBuffers(1,&vb);glDeleteBuffers(1,&ib16);glDeleteBuffers(1,&ib32);glDeleteTextures(1,&texture);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();printf("overall=%s\n",ok?"PASS":"FAIL");return ok?0:1;
}
