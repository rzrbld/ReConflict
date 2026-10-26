/* CPU/GPU pixel comparison. Build command documented in docs/gpu-vertex.md. */
#define _POSIX_C_SOURCE 200809L
#include <SDL.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "d3d8_renderer.h"
/* Optional link wrappers prove unchanged large blocks are not re-uploaded. */
#ifdef DS_TEST_UNIFORM_COUNTS
static unsigned matrix_calls,light_calls,material_calls,small_calls;
void __real_glUniformMatrix4fv(GLint,GLsizei,GLboolean,const GLfloat*);
void __real_glUniformMatrix3fv(GLint,GLsizei,GLboolean,const GLfloat*);
void __real_glUniform4fv(GLint,GLsizei,const GLfloat*);
void __wrap_glUniformMatrix4fv(GLint l,GLsizei n,GLboolean t,const GLfloat *v){matrix_calls++;__real_glUniformMatrix4fv(l,n,t,v);}
void __wrap_glUniformMatrix3fv(GLint l,GLsizei n,GLboolean t,const GLfloat *v){matrix_calls++;__real_glUniformMatrix3fv(l,n,t,v);}
void __wrap_glUniform4fv(GLint l,GLsizei n,const GLfloat *v){if(n==56||n==2)light_calls++;if(n==4)material_calls++;if(n==1)small_calls++;__real_glUniform4fv(l,n,v);}
#ifdef DS_TEST_SMALL_UNIFORM_COUNTS
#define WRAP_UNIFORM(name,params,args) void __real_##name params;void __wrap_##name params{small_calls++;__real_##name args;}
WRAP_UNIFORM(glUniform1i,(GLint l,GLint a),(l,a))
WRAP_UNIFORM(glUniform4i,(GLint l,GLint a,GLint b,GLint c,GLint d),(l,a,b,c,d))
WRAP_UNIFORM(glUniform1f,(GLint l,GLfloat a),(l,a))
WRAP_UNIFORM(glUniform2f,(GLint l,GLfloat a,GLfloat b),(l,a,b))
WRAP_UNIFORM(glUniform3f,(GLint l,GLfloat a,GLfloat b,GLfloat c),(l,a,b,c))
WRAP_UNIFORM(glUniform4f,(GLint l,GLfloat a,GLfloat b,GLfloat c,GLfloat d),(l,a,b,c,d))
WRAP_UNIFORM(glUniform3fv,(GLint l,GLsizei n,const GLfloat *v),(l,n,v))
#undef WRAP_UNIFORM
#endif
#endif
#define W 128
static unsigned char pixels[2][W*W*4];
static void bits(uint32_t *x,float f){memcpy(x,&f,4);}
int main(void){SDL_Init(SDL_INIT_VIDEO);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_Window *w=SDL_CreateWindow("compare",0,0,W,W,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);SDL_GLContext ctx=SDL_GL_CreateContext(w);if(!ctx)return 2;
 D8Renderer r[2];unsetenv("DS_GPU_VERTEX");if(d8_renderer_init(r))return 2;setenv("DS_GPU_VERTEX","1",1);if(d8_renderer_init(r+1)||!r[1].gpu_program)return 2;
 GLuint tex;glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);unsigned char image[]={255,64,32,255,32,255,64,255,64,32,255,255,240,240,240,255};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,image);GLuint textures[]={tex,0};
 struct V {float p[3],normal[3];uint32_t color,spec;float uv[2];} vertices[]={{{9,9,9},{0,0,-1},0,0,{0,0}},{{-.8,-.8,1},{.2,.1,-1},0xffd09060,0x40ffffff,{0,1}},{{.8,-.8,1.4},{.2,.1,-1},0xffa0d090,0x80ffffff,{1,1}},{{0,.8,1.1},{.2,.1,-1},0xff90a0d0,0xc0ffffff,{.5,0}}};uint16_t indices[]={0,1,2};
 int fail=0;for(int test=0;test<10;test++){int rhw=test>=4&&test<8;D8State s;d8_state_defaults(&s,W,W);s.rs[7]=0;s.rs[22]=1;s.fvf=0x1d2;s.rs[137]=test!=0;s.rs[143]=1;s.rs[29]=1;s.rs[142]=test==3;s.rs[139]=0xff404040;s.rs[28]=1;s.rs[140]=3;bits(&s.rs[36],.2f);bits(&s.rs[37],4.f);s.rs[34]=0xff306080;s.material[16]=8.f+test;
 s.material[4]=.6;s.material[5]=.4;s.material[6]=.2;for(int i=8;i<11;i++)s.material[i]=.7;
 s.transform[256][0]=.8;s.transform[256][5]=.9;s.transform[256][1]=.12;s.transform[256][4]=-.15;s.transform[256][12]=.05f+test*.003f;s.transform[2][12]=.06f+test*.004f;s.transform[2][14]=.15;
 memset(s.transform[3],0,64);s.transform[3][0]=s.transform[3][5]=1.f+test*.005f;s.transform[3][10]=1.05;s.transform[3][11]=1;s.transform[3][14]=-.105;
 s.tss[0][24]=2;s.transform[16][0]=.8f+test*.007f;s.transform[16][5]=.9;s.transform[16][12]=.1f+test*.002f;s.transform[17][12]=test*.003f;s.texture_flipy[0]=test==3;
 uint32_t type=test==2?1:test==3?2:3;memcpy(s.light[0],&type,4);s.light_on[0]=1;float *l=s.light[0];l[1]=.7f-test*.01f;l[2]=.8;l[3]=.6;l[5]=.4;l[6]=.3;l[7]=.2;l[9]=.1;l[10]=.1;l[11]=.1;l[13]=.2;l[14]=.3;l[15]=0;l[16]=0;l[17]=0;l[18]=1;l[19]=20;l[20]=2;l[21]=1;l[22]=.1;l[23]=.01;l[24]=.5;l[25]=2.8;
 struct Q {float p[4];uint32_t color,spec;float uv[2];} q[]={{{-.5,W-.5,.5,1},0xffd0a070,0x80ffffff,{0,1}},{{W-.5,W-.5,.5,.8},0xff90d0a0,0x80ffffff,{1,1}},{{W-.5,-.5,.5,1.2},0xffa090d0,0x80ffffff,{1,0}},{{-.5,-.5,.5,.9},0xffa090d0,0x80ffffff,{0,0}}};uint16_t qi[]={0,1,2,0,2,3};if(rhw){s.fvf=0x1c4;s.rs[35]=0;s.rs[140]=0;}
 if(test==5)for(int i=0;i<4;i++)q[i].p[3]=-1;
 if(test==6){for(int i=0;i<4;i++){q[i].p[0]=q[i].p[0]*.5f+W*.25f;q[i].p[1]=q[i].p[1]*.5f+W*.25f;}q[0].p[3]=q[3].p[3]=-1;}
 if(test==7)for(int i=0;i<4;i++)q[i].p[3]=0;
 struct V prefix[23];memset(prefix,0,sizeof prefix);memcpy(prefix+20,vertices+1,3*sizeof vertices[0]);uint16_t shifted16[]={17,18,19};uint32_t shifted32[]={17,18,19};
 const void *draw_data=rhw?(void*)q:test>=8?(void*)prefix:(void*)vertices;
 const void *draw_indices=rhw?(void*)qi:test==8?(void*)shifted16:test==9?(void*)shifted32:(void*)indices;
 unsigned draw_count=rhw?2:1,draw_vertices=rhw?4:test>=8?20:3,draw_base=rhw?0:test>=8?3:1,draw_isize=test==9?4:2;
 size_t draw_stride=rhw?sizeof q[0]:sizeof vertices[0];
 for(int k=0;k<2;k++){glColorMask(1,1,1,1);glClearColor(.1,.2,.3,1);glClear(GL_COLOR_BUFFER_BIT);int error=d8_renderer_draw(r+k,&s,W,4,draw_count,draw_data,draw_stride,draw_vertices,draw_indices,draw_isize,draw_base,textures);if(error){fprintf(stderr,"draw fail %s\n",d8_renderer_error());return 2;}
 if(k==1){
 D8UploadStats stats=d8_renderer_take_upload_stats(r+1);
 if(test>=8){uint64_t expected=(r[1].trim_vertex_prefix?3u:20u)*draw_stride;
 printf("case=%d upload_vertex_bytes=%llu prefix_vertex_bytes=%llu index_bytes=%llu trimmed=%llu\n",test,(unsigned long long)stats.vertex_bytes,(unsigned long long)stats.prefix_vertex_bytes,(unsigned long long)stats.index_bytes,(unsigned long long)stats.trimmed_draws);
 if(stats.vertex_bytes!=expected||stats.index_bytes!=3*draw_isize||stats.prefix_vertex_bytes!=20*draw_stride||stats.avoidable_prefix_bytes!=17*draw_stride||stats.vertex_uploads!=1||stats.index_uploads!=1||stats.trimmed_draws!=(unsigned)(r[1].trim_vertex_prefix!=0)){fprintf(stderr,"upload stats mismatch case %d vertices=%llu expected=%llu\n",test,(unsigned long long)stats.vertex_bytes,(unsigned long long)expected);fail=1;}}

 }
#ifdef DS_TEST_UNIFORM_COUNTS
 matrix_calls=light_calls=material_calls=small_calls=0;
#endif
 /* Another program becoming current cannot invalidate this program's uniforms. */
 glUseProgram(k==0?r[1].gpu_program:r[0].program);
 if(d8_renderer_draw(r+k,&s,W,4,draw_count,draw_data,draw_stride,draw_vertices,draw_indices,draw_isize,draw_base,textures))return 2;
#ifdef DS_TEST_UNIFORM_COUNTS
 if(matrix_calls||light_calls||material_calls||small_calls){fprintf(stderr,"repeat backend=%d uploads matrices=%u lights=%u material=%u small=%u\n",k,matrix_calls,light_calls,material_calls,small_calls);fail=1;}
#endif
 glReadPixels(0,0,W,W,GL_RGBA,GL_UNSIGNED_BYTE,pixels[k]);if(k==1)d8_renderer_take_upload_stats(r+1);}
 unsigned bad=0,max=0,paint=0;for(unsigned i=0;i<sizeof pixels[0];i++){unsigned d=abs((int)pixels[0][i]-pixels[1][i]);if(d>max)max=d;if(d>2)bad++;if(i%4==0&&pixels[0][i]>40)paint++;}printf("case=%d maxdiff=%u channels_gt2=%u painted=%u\n",test,max,bad,paint);if(bad>4||(test==5?paint!=0:paint<100))fail=1;
 }
 /* Nonfinite RHW remains an actionable error on both paths. */
 D8State bad;d8_state_defaults(&bad,W,W);bad.fvf=0x44;bad.rs[22]=1;struct {float p[4];uint32_t c;} invalid[3]={{{0,0,.5,NAN},~0u},{{64,0,.5,1},~0u},{{0,64,.5,1},~0u}};
 for(int k=0;k<2;k++)if(d8_renderer_draw(r+k,&bad,W,4,1,invalid,sizeof invalid[0],3,NULL,0,0,textures)==0||!strstr(d8_renderer_error(),"RHW"))fail=1;
 printf("nonfinite_RHW=%s\n",fail?"FAIL":"PASS");
 /* A failed GL submission invalidates all uniform snapshots for that renderer.
  * Recovery must upload again, rather than retaining a possibly stale cache. */
 invalid[0].p[3]=1;bad.rs[7]=0;
 for(int k=0;k<2;k++){
  if(d8_renderer_draw(r+k,&bad,W,4,1,invalid,sizeof invalid[0],3,NULL,0,0,textures))return 2;
  glEnable(0xdeadbeefu);
  if(!d8_renderer_draw(r+k,&bad,W,4,1,invalid,sizeof invalid[0],3,NULL,0,0,textures)||r[k].small_uniforms.valid||r[k].gpu_small_uniforms.valid||r[k].gpu_uploaded.valid)fail=1;
#ifdef DS_TEST_UNIFORM_COUNTS
  matrix_calls=light_calls=material_calls=small_calls=0;
#endif
  if(d8_renderer_draw(r+k,&bad,W,4,1,invalid,sizeof invalid[0],3,NULL,0,0,textures))return 2;
#ifdef DS_TEST_UNIFORM_COUNTS
  if(!small_calls||(k==1&&!matrix_calls)){fprintf(stderr,"recovery did not reupload backend=%d small=%u matrices=%u\n",k,small_calls,matrix_calls);fail=1;}
#endif
 }
 printf("GL_error_cache_invalidation_and_recovery=%s\n",fail?"FAIL":"PASS");

#ifdef DS_TEST_UNIFORM_COUNTS
 printf("unchanged_CPU_GPU_uploads_with_program_switch=%s\n",fail?"FAIL":"PASS");
#endif
 d8_renderer_close(r);d8_renderer_close(r+1);SDL_GL_DeleteContext(ctx);SDL_DestroyWindow(w);SDL_Quit();return fail;
}
