#include <SDL.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "d3d8_renderer.h"
static unsigned active_calls,bind_calls;
static int default_framebuffer=1,verify_empty_samplers,sampler_failure;
void __real_glActiveTexture(GLenum);
void __wrap_glActiveTexture(GLenum unit){active_calls++;__real_glActiveTexture(unit);}
void __real_glBindTexture(GLenum,GLuint);
void __wrap_glBindTexture(GLenum target,GLuint texture){bind_calls++;__real_glBindTexture(target,texture);}
void __real_glDrawArrays(GLenum,GLint,GLsizei);
void __wrap_glDrawArrays(GLenum mode,GLint first,GLsizei count){
 if(verify_empty_samplers){
  GLint active;glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
  for(unsigned i=0;i<2;i++){GLint bound;__real_glActiveTexture(GL_TEXTURE0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);if(bound)sampler_failure=1;}
  __real_glActiveTexture(active);
 }
 __real_glDrawArrays(mode,first,count);
}
static struct {float p[4];uint32_t color;float uv[2];} quad[]={
 {{-.5,-.5,.5,1},~0u,{0,0}},{{63.5,-.5,.5,1},~0u,{1,0}},{{63.5,63.5,.5,1},~0u,{1,1}},{{-.5,63.5,.5,1},~0u,{0,1}}};
static int check(const char *name,D8Renderer *r,D8State *s,GLuint a,GLuint b,unsigned calls,unsigned red,unsigned green,unsigned blue){
 GLuint textures[]={a,b};active_calls=bind_calls=0;
 D8NativeBuffers native={0};native.default_framebuffer=default_framebuffer;
 int rc=d8_renderer_draw_native(r,s,64,6,2,quad,sizeof quad[0],4,NULL,0,0,textures,&native);
 unsigned char pixel[4];glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
 int pass=!rc&&active_calls==calls&&bind_calls==calls&&abs((int)pixel[0]-(int)red)<=1&&abs((int)pixel[1]-(int)green)<=1&&abs((int)pixel[2]-(int)blue)<=1;
 printf("%s=%s ActiveTexture=%u BindTexture=%u pixel=%u,%u,%u\n",name,pass?"PASS":"FAIL",active_calls,bind_calls,pixel[0],pixel[1],pixel[2]);return !pass;
}
int main(void){
 if(SDL_Init(SDL_INIT_VIDEO))return 2;SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
 SDL_Window *w=SDL_CreateWindow("inactive texture test",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);SDL_GLContext ctx=w?SDL_GL_CreateContext(w):NULL;if(!ctx)return 2;
 D8Renderer renderer,other;if(d8_renderer_init(&renderer)||d8_renderer_init(&other))return 2;
 GLuint textures[3];glGenTextures(3,textures);
 for(unsigned i=0;i<3;i++){unsigned char p[4]={0,0,0,255};p[i]=255;glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,textures[i]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,p);}
 D8State s;d8_state_defaults(&s,64,64);s.fvf=0x144;s.rs[7]=0;s.rs[22]=1;s.rs[137]=0;
 int fail=check("one_texture_binds_only_unit0",&renderer,&s,textures[0],0,1,255,0,0);
 fail|=check("steady_one_texture",&renderer,&s,textures[0],0,1,255,0,0);
 s.tss[1][1]=2;s.tss[1][2]=2;s.tss[1][4]=2;s.tss[1][5]=2;
 fail|=check("two_active_units",&renderer,&s,textures[0],textures[2],2,0,0,255);
 s.tss[1][1]=1;
 fail|=check("inactive_unit_keeps_stale_blue_binding_without_sampling",&renderer,&s,textures[0],textures[2],1,255,0,0);
 glActiveTexture(GL_TEXTURE1);GLint bound;glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);if(bound!=(GLint)textures[2])fail=1;
 s.tss[0][1]=1;glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,textures[1]);
 fail|=check("all_disabled_keeps_stale_green_without_sampling",&renderer,&s,textures[0],textures[2],0,255,255,255);
 s.tss[0][1]=2;s.tss[0][2]=0;s.tss[0][4]=2;s.tss[0][5]=0;
 fail|=check("enabled_untextured_diffuse_ignores_bound_green",&renderer,&s,0,0,0,255,255,255);
 s.tss[0][2]=2;s.tss[0][5]=2;
 fail|=check("null_texture_terminates_stage_without_bind",&renderer,&s,0,textures[2],0,255,255,255);
 fail|=check("active_texture_restores_from_external_mutation",&renderer,&s,textures[0],0,1,255,0,0);
 fail|=check("other_renderer_uses_its_active_binding",&other,&s,textures[1],0,1,0,255,0);
 fail|=check("interleaved_renderer_restores_active_binding",&renderer,&s,textures[0],0,1,255,0,0);
 glEnable(0xdeadbeefu);GLuint t[]={textures[0],0};if(!d8_renderer_draw(&renderer,&s,64,6,2,quad,sizeof quad[0],4,NULL,0,0,t))fail=1;
 fail|=check("GL_error_recovery_binds_active_unit",&renderer,&s,textures[0],0,1,255,0,0);
 /* ES2 feedback rules ignore conditional sampling. On an offscreen target
  * the backend must still clear stale attachment bindings BEFORE the draw. */
 GLuint attachment,fbo;glGenTextures(1,&attachment);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,attachment);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
 glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,attachment,0);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)return 2;
 glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,attachment);s.tss[0][1]=1;default_framebuffer=0;verify_empty_samplers=1;
 fail|=check("offscreen_target_clears_both_stale_attachment_bindings",&renderer,&s,0,0,2,255,255,255);if(sampler_failure)fail=1;verify_empty_samplers=0;
 s.tss[0][1]=2;fail|=check("offscreen_textured_keeps_original_two_bind_behavior",&renderer,&s,textures[0],0,2,255,0,0);
 glBindFramebuffer(GL_FRAMEBUFFER,0);default_framebuffer=1;s.tss[0][1]=1;
 fail|=check("return_to_default_target_restores_no_bind_path",&renderer,&s,0,0,0,255,255,255);
 glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&attachment);
 printf("backend=%s overall=%s\n",renderer.gpu_program?"GPU":"CPU",fail?"FAIL":"PASS");
 d8_renderer_close(&renderer);d8_renderer_close(&other);glDeleteTextures(3,textures);SDL_GL_DeleteContext(ctx);SDL_DestroyWindow(w);SDL_Quit();return fail;
}
