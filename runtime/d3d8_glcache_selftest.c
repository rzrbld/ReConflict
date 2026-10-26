#define _POSIX_C_SOURCE 200809L
#include <SDL.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <string.h>
#include "d3d8_renderer.h"
#ifdef DS_TEST_GL_CACHE_CALLS
static unsigned texture_calls,attribute_calls;
#define WRAP(name,params,args,kind) void __real_##name params;void __wrap_##name params{kind++;__real_##name args;}
WRAP(glTexParameteri,(GLenum a,GLenum b,GLint c),(a,b,c),texture_calls)
WRAP(glVertexAttribPointer,(GLuint a,GLint b,GLenum c,GLboolean d,GLsizei e,const void *f),(a,b,c,d,e,f),attribute_calls)
WRAP(glEnableVertexAttribArray,(GLuint a),(a),attribute_calls)
WRAP(glDisableVertexAttribArray,(GLuint a),(a),attribute_calls)
WRAP(glVertexAttrib4f,(GLuint a,GLfloat b,GLfloat c,GLfloat d,GLfloat e),(a,b,c,d,e),attribute_calls)
#undef WRAP
#else
static unsigned texture_calls,attribute_calls;
#endif
static struct {float p[4];uint32_t color;float uv[2];} quad[]={{{-.5,-.5,.5,1},~0u,{.6,.6}},{{63.5,-.5,.5,1},~0u,{.6,.6}},{{63.5,63.5,.5,1},~0u,{.6,.6}},{{-.5,63.5,.5,1},~0u,{.6,.6}}};
static struct {float p[3],n[3];uint32_t color;float uv[2];} xyz[]={{{-1,1,.5},{0,0,1},~0u,{.6,.6}},{{1,1,.5},{0,0,1},~0u,{.6,.6}},{{1,-1,.5},{0,0,1},~0u,{.6,.6}},{{-1,-1,.5},{0,0,1},~0u,{.6,.6}}};
static int draw(D8Renderer *r,D8State *s,GLuint a,GLuint b){GLuint tex[]={a,b};return d8_renderer_draw(r,s,64,6,2,s->fvf==0x152?(void*)xyz:(void*)quad,s->fvf==0x152?sizeof xyz[0]:sizeof quad[0],4,NULL,0,0,tex);}
static int pixel(int r,int g,int b){unsigned char p[4];glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p);return abs((int)p[0]-r)<=2&&abs((int)p[1]-g)<=2&&abs((int)p[2]-b)<=2;}
static int counts(unsigned tex,unsigned attrs){
#ifdef DS_TEST_GL_CACHE_CALLS
 return texture_calls==tex&&attribute_calls==attrs;
#else
 (void)tex;(void)attrs;return 1;
#endif
}
#define RESET() (texture_calls=attribute_calls=0)
#define CHECK(name,condition) do{int pass=(condition);printf("%s=%s texture_calls=%u attribute_calls=%u\n",name,pass?"PASS":"FAIL",texture_calls,attribute_calls);if(!pass)failed=1;}while(0)
int main(void){
 if(SDL_Init(SDL_INIT_VIDEO))return 2;SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_Window *w=SDL_CreateWindow("GL cache test",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);if(!w)return 2;SDL_GLContext context=SDL_GL_CreateContext(w);if(!context)return 2;
 D8Renderer r,other;if(d8_renderer_init(&r)||d8_renderer_init(&other))return 2;D8State s;d8_state_defaults(&s,64,64);s.fvf=0x144;s.rs[7]=0;s.rs[22]=1;s.rs[137]=0;s.tss[0][13]=s.tss[0][14]=3;
 unsigned char rgba[]={255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};GLuint tex;glGenTextures(1,&tex);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,tex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);d8_renderer_forget_texture(&r,tex);
 int failed=0;RESET();int rc=draw(&r,&s,tex,0);CHECK("new_texture_initializes_four_parameters",!rc&&texture_calls==4&&pixel(255,255,255));
 RESET();CHECK("identical_draw_skips_parameters_and_attributes",!draw(&r,&s,tex,0)&&counts(0,0)&&pixel(255,255,255));
 s.tss[0][16]=2;RESET();CHECK("only_changed_MAG_parameter_submits",!draw(&r,&s,tex,0)&&counts(1,0)&&pixel(148,179,179));
 s.tss[0][17]=2;RESET();CHECK("only_changed_MIN_parameter_submits",!draw(&r,&s,tex,0)&&counts(1,0)&&pixel(148,179,179));
 for(int i=0;i<4;i++)quad[i].uv[0]=1.6;s.tss[0][13]=1;RESET();CHECK("buffer_upload_does_not_invalidate_attributes",!draw(&r,&s,tex,0)&&counts(1,0)&&pixel(148,179,179));
 s.tss[0][13]=3;RESET();CHECK("changed_wrap_has_correct_pixels",!draw(&r,&s,tex,0)&&counts(1,0)&&pixel(179,255,179));
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,tex);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);d8_renderer_forget_texture(&r,tex);RESET();CHECK("external_parameter_mutation_forget_id",!draw(&r,&s,tex,0)&&counts(4,0)&&pixel(179,255,179));
 unsigned char flat[16];for(int i=0;i<4;i++){flat[i*4]=64;flat[i*4+1]=128;flat[i*4+2]=192;flat[i*4+3]=255;}glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,tex);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,2,2,GL_RGBA,GL_UNSIGNED_BYTE,flat);RESET();CHECK("pixel_upload_preserves_parameter_cache",!draw(&r,&s,tex,0)&&counts(0,0)&&pixel(64,128,192));
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,tex);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,2,2,GL_RGBA,GL_UNSIGNED_BYTE,rgba);for(int i=0;i<4;i++)quad[i].uv[0]=.6;
 for(int j=0;j<32;j++)s.tss[1][j]=s.tss[0][j];s.tss[1][1]=2;s.tss[1][2]=2;RESET();CHECK("same_object_on_two_units_shares_parameter_cache",!draw(&r,&s,tex,tex)&&counts(0,r.gpu_program?2:0)&&pixel(148,179,179));
 RESET();CHECK("steady_two_unit_draw_skips_all_cached_calls",!draw(&r,&s,tex,tex)&&counts(0,0)&&pixel(148,179,179));
 s.tss[0][16]=1;RESET();CHECK("same_object_different_unit_states_track_actual_order",!draw(&r,&s,tex,tex)&&texture_calls==2&&pixel(148,179,179));
 s.tss[1][1]=1;RESET();CHECK("unit_disable_restores_object_parameter_needed_by_unit0",!draw(&r,&s,tex,0)&&texture_calls==1&&pixel(255,255,255));
 GLuint fake;glGenBuffers(1,&fake);glBindBuffer(GL_ARRAY_BUFFER,fake);glBufferData(GL_ARRAY_BUFFER,sizeof quad,quad,GL_STATIC_DRAW);glVertexAttribPointer(0,2,GL_FLOAT,0,8,0);glDisableVertexAttribArray(0);glVertexAttrib4f(1,9,8,7,6);glEnableVertexAttribArray(1);d8_renderer_invalidate_attributes(&r);RESET();rc=draw(&r,&s,tex,0);CHECK("external_attrib_mutation_invalidation_restores_pixels",!rc&&attribute_calls>0&&pixel(255,255,255));
 if(r.gpu_program){s.fvf=0x152;if(draw(&r,&s,tex,0))return 2;s.fvf=0x144;RESET();rc=draw(&r,&s,tex,0);float normal[4];glGetVertexAttribfv(1,GL_CURRENT_VERTEX_ATTRIB,normal);CHECK("array_to_constant_reapplies_undefined_current_value",!rc&&normal[0]==0&&normal[1]==0&&normal[2]==1&&normal[3]==0&&pixel(255,255,255));
  GLuint saved=r.gpu_program;r.gpu_program=0;if(draw(&r,&s,tex,0))return 2;r.gpu_program=saved;RESET();CHECK("same_renderer_CPU_to_GPU_attribute_transition",!draw(&r,&s,tex,0)&&attribute_calls>0&&pixel(255,255,255));}
 RESET();CHECK("steady_after_attribute_transitions_skips_calls",!draw(&r,&s,tex,0)&&counts(0,0)&&pixel(255,255,255));
 D8State theirs=s;theirs.tss[0][16]=2;if(draw(&other,&theirs,tex,0))return 2;RESET();CHECK("interleaved_renderer_invalidates_object_and_attrib_ownership",!draw(&r,&s,tex,0)&&texture_calls==4&&attribute_calls>0&&pixel(255,255,255));
 glEnable(0xdeadbeefu);rc=draw(&r,&s,tex,0);RESET();CHECK("GL_error_invalidation_recovery",rc!=0&&!draw(&r,&s,tex,0)&&texture_calls==4&&attribute_calls>0&&pixel(255,255,255));
 d8_renderer_forget_texture(&r,tex);glDeleteTextures(1,&tex);GLuint old=tex;glGenTextures(1,&tex);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,tex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);d8_renderer_forget_texture(&r,tex);RESET();CHECK("texture_delete_create_lifecycle_reinitializes_parameters",!draw(&r,&s,tex,0)&&texture_calls==4&&pixel(255,255,255));printf("texture_names old=%u new=%u\n",old,tex);
 d8_renderer_invalidate_texture_parameters(&r);RESET();CHECK("whole_parameter_cache_invalidation",!draw(&r,&s,tex,0)&&texture_calls==4&&pixel(255,255,255));
 glDeleteBuffers(1,&fake);d8_renderer_invalidate_attributes(&r);d8_renderer_forget_texture(&r,tex);glDeleteTextures(1,&tex);printf("backend=%s overall=%s\n",r.gpu_program?"GPU":"CPU",failed?"FAIL":"PASS");d8_renderer_close(&other);d8_renderer_close(&r);SDL_GL_DeleteContext(context);SDL_DestroyWindow(w);SDL_Quit();return failed;
}
