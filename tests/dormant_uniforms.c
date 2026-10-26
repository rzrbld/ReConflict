#define _POSIX_C_SOURCE 200809L
#include <SDL.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "d3d8_renderer.h"
extern int baseline_d8_renderer_init(D8Renderer *);
extern int baseline_d8_renderer_draw(D8Renderer *,const D8State *,unsigned,unsigned,unsigned,const void *,size_t,unsigned,const void *,unsigned,unsigned,const GLuint[2]);
extern void baseline_d8_renderer_close(D8Renderer *);
extern void baseline_d8_renderer_invalidate_raster(D8Renderer *);
extern void baseline_d8_renderer_invalidate_attributes(D8Renderer *);
extern void baseline_d8_renderer_invalidate_texture_parameters(D8Renderer *);
static D8Renderer candidate;
static GLuint current;
static unsigned matrix_calls,lighting_calls,program_calls;
static void location(GLint l){
 if(current!=candidate.gpu_program)return;
 D8UniformLocations *u=&candidate.gpu_uniforms;
 if(l==u->world||l==u->view||l==u->projection||l==u->normalmat||l==u->eye||l==u->camera)matrix_calls++;
 if(l==u->material||l==u->power||l==u->ambient||l==u->sources||l==u->localviewer||l==u->lights||l==u->enabled)lighting_calls++;
}
#define WRAP(name,params,args,l) void __real_##name params;void __wrap_##name params{location(l);__real_##name args;}
WRAP(glUniformMatrix4fv,(GLint l,GLsizei n,GLboolean t,const GLfloat *v),(l,n,t,v),l)
WRAP(glUniformMatrix3fv,(GLint l,GLsizei n,GLboolean t,const GLfloat *v),(l,n,t,v),l)
WRAP(glUniform4fv,(GLint l,GLsizei n,const GLfloat *v),(l,n,v),l)
WRAP(glUniform3fv,(GLint l,GLsizei n,const GLfloat *v),(l,n,v),l)
WRAP(glUniform1f,(GLint l,GLfloat v),(l,v),l)
WRAP(glUniform1i,(GLint l,GLint v),(l,v),l)
WRAP(glUniform4i,(GLint l,GLint a,GLint b,GLint c,GLint d),(l,a,b,c,d),l)
#undef WRAP
void __real_glUseProgram(GLuint);
void __wrap_glUseProgram(GLuint p){current=p;if(p==candidate.gpu_program)program_calls++;__real_glUseProgram(p);}
#define W 64
static unsigned char image[3][W*W*4],previous[W*W*4];
static struct X {float p[3],normal[3];uint32_t color,spec;float uv[2];} xyz[]={
 {{-.8,-.8,.5},{0,0,-1},0xff80b0e0,0,{0,1}},{{.8,-.8,.5},{0,0,-1},0xffb0e080,0,{1,1}},{{0,.8,.5},{0,0,-1},0xffe080b0,0,{.5,0}}};
static struct Q {float p[4];uint32_t color,spec;float uv[2];} quad[]={
 {{-.5,63.5,.5,1},0xff80b0e0,0,{0,1}},{{63.5,63.5,.5,1},0xffb0e080,0,{1,1}},{{-.5,-.5,.5,1},0xffe080b0,0,{0,0}}};
static struct N {float p[3];uint32_t color,spec;float uv[2];} no_normal[3];
static void bits(uint32_t *dst,float f){memcpy(dst,&f,4);}
static int submit(D8Renderer *r,D8State *s,GLuint *tex,int baseline){
 const void *data=s->fvf==0x1c4?(void*)quad:s->fvf==0x1c2?(void*)no_normal:(void*)xyz;
 size_t stride=s->fvf==0x1c4?sizeof quad[0]:s->fvf==0x1c2?sizeof no_normal[0]:sizeof xyz[0];
 return baseline?baseline_d8_renderer_draw(r,s,W,4,1,data,stride,3,NULL,0,0,tex):d8_renderer_draw(r,s,W,4,1,data,stride,3,NULL,0,0,tex);
}
static int run(const char *name,D8Renderer *cpu,D8Renderer *baseline,D8State *s,GLuint *tex,int matrices,int lighting,int unchanged){
 D8Renderer *renderer[]={cpu,baseline,&candidate};
 for(int k=0;k<3;k++){
  glClearColor(.03,.02,.01,1);glClear(GL_COLOR_BUFFER_BIT);
  /* Baseline is a separate renderer translation unit: its context-owner
   * globals cannot observe draws from the candidate translation unit. */
  if(k==1){baseline_d8_renderer_invalidate_raster(baseline);baseline_d8_renderer_invalidate_attributes(baseline);baseline_d8_renderer_invalidate_texture_parameters(baseline);}
  if(k==2){matrix_calls=lighting_calls=program_calls=0;glUseProgram(0);}
  if(submit(renderer[k],s,tex,k==1)){fprintf(stderr,"%s draw failed backend%d: %s\n",name,k,d8_renderer_error());return 1;}
  glReadPixels(0,0,W,W,GL_RGBA,GL_UNSIGNED_BYTE,image[k]);
 }
 unsigned max_cpu=0,bad=0,paint=0;
 for(unsigned i=0;i<sizeof image[0];i++){
  unsigned d=abs((int)image[0][i]-image[2][i]);if(d>max_cpu)max_cpu=d;if(d>2)bad++;
  if(i%4==0&&image[2][i]>20)paint++;
 }
 int pass=!memcmp(image[1],image[2],sizeof image[1])&&!bad&&paint>100&&program_calls==1;
 pass&=matrices<0||!!matrix_calls==matrices;pass&=lighting<0||!!lighting_calls==lighting;
 if(unchanged)pass&=!memcmp(previous,image[2],sizeof previous);
 memcpy(previous,image[2],sizeof previous);
 printf("%s=%s cpu_maxdiff=%u baseline_exact=%d painted=%u matrix_calls=%u lighting_calls=%u program_calls=%u\n",name,pass?"PASS":"FAIL",max_cpu,!memcmp(image[1],image[2],sizeof image[1]),paint,matrix_calls,lighting_calls,program_calls);
 return !pass;
}
static void mutate_lighting(D8State *s,unsigned n){
 s->material[0]=.4f+n*.04f;s->material[1]=.5f+n*.02f;s->material[2]=.6f-n*.02f;s->material[16]=3+n;
 s->rs[139]=0xff203040u+n*0x020202;s->rs[142]=n&1;s->rs[29]=n&1;
 uint32_t type=n&1?1:3;memcpy(s->light[0],&type,4);
 s->light[0][1]=.4f+n*.03f;s->light[0][2]=.6f-n*.02f;s->light[0][3]=.7f;
 s->light[0][13]=n*.03f;s->light[0][15]=-1;s->light[0][16]=0;s->light[0][17]=0;s->light[0][18]=1;s->light[0][19]=20;s->light[0][21]=1;
 s->light_on[0]=1;s->light_on[1]=n&1;
 s->rs[145]=n&1?0:1; /* material versus vertex diffuse */
}
int main(void){
 if(SDL_Init(SDL_INIT_VIDEO))return 2;SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
 SDL_Window *window=SDL_CreateWindow("dormant uniforms",0,0,W,W,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);SDL_GLContext context=window?SDL_GL_CreateContext(window):NULL;if(!context)return 2;
 D8Renderer cpu,baseline;unsetenv("DS_GPU_VERTEX");if(d8_renderer_init(&cpu))return 2;setenv("DS_GPU_VERTEX","1",1);if(baseline_d8_renderer_init(&baseline)||d8_renderer_init(&candidate)||!candidate.gpu_program)return 2;
 D8State s;d8_state_defaults(&s,W,W);s.rs[7]=0;s.rs[22]=1;s.rs[28]=1;s.rs[140]=3;s.rs[34]=0xff283848;bits(&s.rs[36],0);bits(&s.rs[37],3);s.fvf=0x1c4;
 for(int i=0;i<3;i++){memcpy(no_normal[i].p,xyz[i].p,12);no_normal[i].color=xyz[i].color;no_normal[i].spec=xyz[i].spec;memcpy(no_normal[i].uv,xyz[i].uv,8);}
 GLuint tex[2]={0};glGenTextures(1,tex);glBindTexture(GL_TEXTURE_2D,tex[0]);unsigned char pixels[]={255,80,60,255,60,255,80,255,80,60,255,255,200,220,240,255};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
 int fail=run("initial_RHW_leaves_large_snapshots_invalid",&cpu,&baseline,&s,tex,0,0,0);
 if(candidate.gpu_uploaded.valid&0x3e7u)fail=1;
 D8GpuUploadCache snapshot=candidate.gpu_uploaded;
 s.transform[256][12]=.07f;s.transform[2][13]=.03f;s.transform[3][0]=.9f;mutate_lighting(&s,1);
 fail|=run("RHW_mutations_deferred",&cpu,&baseline,&s,tex,0,0,0);
 if(memcmp(&snapshot,&candidate.gpu_uploaded,sizeof snapshot))fail=1;
 s.fvf=0x1d2;s.rs[137]=0;
 fail|=run("XYZ_unlit_uploads_matrices_only",&cpu,&baseline,&s,tex,1,0,0);
 snapshot=candidate.gpu_uploaded;mutate_lighting(&s,2);
 fail|=run("unlit_lighting_mutations_deferred",&cpu,&baseline,&s,tex,0,0,1);
 if(memcmp(&snapshot,&candidate.gpu_uploaded,sizeof snapshot))fail=1;
 s.rs[137]=1;
 fail|=run("lighting_enable_uploads_deferred_state",&cpu,&baseline,&s,tex,0,1,0);
 fail|=run("steady_lit_skips_large_uploads",&cpu,&baseline,&s,tex,0,0,1);
 mutate_lighting(&s,3);fail|=run("active_lighting_mutation",&cpu,&baseline,&s,tex,0,1,0);
 s.rs[137]=0;s.transform[256][13]=.08f;s.transform[2][12]=-.02f;s.transform[3][5]=.95f;mutate_lighting(&s,4);
 fail|=run("unlit_matrices_change_lighting_deferred",&cpu,&baseline,&s,tex,1,0,0);
 snapshot=candidate.gpu_uploaded;s.fvf=0x1c4;s.transform[256][0]=.85f;s.transform[2][13]=-.04f;s.transform[3][0]=.8f;mutate_lighting(&s,5);
 fail|=run("RHW_after_XYZ_preserves_large_snapshots",&cpu,&baseline,&s,tex,0,0,0);
 if(memcmp(&snapshot,&candidate.gpu_uploaded,sizeof snapshot))fail=1;
 s.fvf=0x1d2;s.rs[137]=1;
 fail|=run("XYZ_lit_reentry_uploads_all_deferred_changes",&cpu,&baseline,&s,tex,1,1,0);
 s.fvf=0x1c2;snapshot=candidate.gpu_uploaded;mutate_lighting(&s,6);
 fail|=run("missing_normal_defers_lighting_even_when_enabled",&cpu,&baseline,&s,tex,0,0,0);
 if(memcmp(&snapshot,&candidate.gpu_uploaded,sizeof snapshot))fail=1;
 s.fvf=0x1d2;fail|=run("normal_return_uploads_deferred_lights",&cpu,&baseline,&s,tex,0,1,0);
 glEnable(0xdeadbeefu);if(!submit(&candidate,&s,tex,0)||candidate.gpu_uploaded.valid)fail=1;
 s.fvf=0x1c4;fail|=run("error_recovery_RHW_still_defers",&cpu,&baseline,&s,tex,0,0,0);
 if(candidate.gpu_uploaded.valid&0x3e7u)fail=1;
 s.fvf=0x1d2;fail|=run("error_recovery_lit_XYZ_reuploads",&cpu,&baseline,&s,tex,1,1,0);
 printf("dormant_snapshots_and_pixels=%s\n",fail?"FAIL":"PASS");
 d8_renderer_close(&cpu);baseline_d8_renderer_close(&baseline);d8_renderer_close(&candidate);glDeleteTextures(1,tex);SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return fail;
}
