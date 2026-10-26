#include <SDL.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <string.h>
#include "d3d8_renderer.h"
#ifdef DS_TEST_RASTER_CALLS
static unsigned raster_calls;
#define WRAP(name,params,args) void __real_##name params;void __wrap_##name params{raster_calls++;__real_##name args;}
WRAP(glEnable,(GLenum a),(a)) WRAP(glDisable,(GLenum a),(a))
WRAP(glDepthMask,(GLboolean a),(a)) WRAP(glDepthFunc,(GLenum a),(a))
WRAP(glBlendFunc,(GLenum a,GLenum b),(a,b)) WRAP(glBlendEquation,(GLenum a),(a))
WRAP(glCullFace,(GLenum a),(a)) WRAP(glFrontFace,(GLenum a),(a))
WRAP(glStencilFunc,(GLenum a,GLint b,GLuint c),(a,b,c))
WRAP(glStencilOp,(GLenum a,GLenum b,GLenum c),(a,b,c)) WRAP(glStencilMask,(GLuint a),(a))
WRAP(glColorMask,(GLboolean a,GLboolean b,GLboolean c,GLboolean d),(a,b,c,d))
WRAP(glPolygonOffset,(GLfloat a,GLfloat b),(a,b))
WRAP(glViewport,(GLint a,GLint b,GLsizei c,GLsizei d),(a,b,c,d))
WRAP(glDepthRangef,(GLfloat a,GLfloat b),(a,b))
#undef WRAP
#else
static unsigned raster_calls;
#endif
static struct {float p[4];uint32_t c;} quad[]={{{-.5,-.5,.5,1},0xffff0000},{{63.5,-.5,.5,1},0xffff0000},{{63.5,63.5,.5,1},0xffff0000},{{-.5,63.5,.5,1},0xffff0000}};
static int draw(D8Renderer *r,D8State *s,unsigned height,uint32_t color){for(int i=0;i<4;i++)quad[i].c=color;GLuint textures[2]={0};return d8_renderer_draw(r,s,height,6,2,quad,sizeof quad[0],4,NULL,0,0,textures);}
static int pixel(unsigned x,unsigned y,unsigned r,unsigned g,unsigned b){unsigned char p[4];glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p);return p[0]==r&&p[1]==g&&p[2]==b;}
static int calls(unsigned expected){
#ifdef DS_TEST_RASTER_CALLS
 return raster_calls==expected;
#else
 (void)expected;return 1;
#endif
}
static int reapplied(void){
#ifdef DS_TEST_RASTER_CALLS
 return raster_calls>0;
#else
 return 1;
#endif
}
#define CHECK(name,condition) do{int pass=(condition);printf("%s=%s raster_calls=%u\n",name,pass?"PASS":"FAIL",raster_calls);if(!pass)failed=1;}while(0)
int main(void){
 if(SDL_Init(SDL_INIT_VIDEO))return 2;SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,8);
 SDL_Window *w=SDL_CreateWindow("raster cache",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);if(!w)return 2;SDL_GLContext gl=SDL_GL_CreateContext(w);if(!gl)return 2;
 D8Renderer r,other;if(d8_renderer_init(&r)||d8_renderer_init(&other))return 2;D8State s;d8_state_defaults(&s,64,64);s.fvf=0x44;s.rs[7]=0;s.rs[22]=1;
 glColorMask(1,1,1,1);glDepthMask(1);glStencilMask(~0u);glClearColor(0,0,1,1);glClearDepthf(1);glClearStencil(0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);d8_renderer_invalidate_raster(&r);
 int failed=0;if(draw(&r,&s,64,0xffff0000))return 2;raster_calls=0;
 CHECK("identical_draw_skips_all_raster_calls",!draw(&r,&s,64,0xffff0000)&&calls(0)&&pixel(32,32,255,0,0));
 s.rs[168]=2;raster_calls=0;CHECK("color_mask_change_only",!draw(&r,&s,64,0xffffff00)&&calls(1)&&pixel(32,32,255,255,0));
 s.rs[168]=4;raster_calls=0;CHECK("second_color_mask_change_only",!draw(&r,&s,64,0xff0000ff)&&calls(1)&&pixel(32,32,255,255,255));
 s.rs[168]=15;s.viewport[2]=32;raster_calls=0;CHECK("viewport_and_mask_change",!draw(&r,&s,64,0xff0000ff)&&calls(2)&&pixel(16,32,0,0,255)&&pixel(48,32,255,255,255));
 s.viewport[3]=32;if(draw(&r,&s,64,0xff00ff00))return 2;raster_calls=0;CHECK("target_height_is_part_of_viewport_key",!draw(&r,&s,80,0xffff0000)&&calls(1)&&pixel(16,40,0,255,0)&&pixel(16,56,255,0,0));
 float near=.1f,far=.9f;memcpy(&s.viewport[4],&near,4);memcpy(&s.viewport[5],&far,4);raster_calls=0;int rc=draw(&r,&s,80,0xffff0000);float range[2];glGetFloatv(GL_DEPTH_RANGE,range);CHECK("depth_range_change_only",!rc&&calls(1)&&range[0]>.099f&&range[0]<.101f&&range[1]>.899f&&range[1]<.901f);
 d8_state_defaults(&s,64,64);s.fvf=0x44;s.rs[7]=0;s.rs[22]=1;if(draw(&r,&s,64,0xffff0000))return 2;
 glColorMask(0,0,0,0);glDepthMask(0);glDepthFunc(GL_NEVER);glEnable(GL_DEPTH_TEST);glEnable(GL_BLEND);glBlendFunc(GL_ZERO,GL_ZERO);glEnable(GL_CULL_FACE);glCullFace(GL_FRONT_AND_BACK);glEnable(GL_STENCIL_TEST);glStencilFunc(GL_NEVER,0,~0u);glEnable(GL_POLYGON_OFFSET_FILL);glViewport(0,0,1,1);glDepthRangef(.2,.3);
 d8_renderer_invalidate_raster(&r);raster_calls=0;rc=draw(&r,&s,64,0xffff0000);GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);CHECK("external_mutation_explicit_invalidation_restores_pixels",!rc&&reapplied()&&pixel(32,32,255,0,0)&&!glIsEnabled(GL_DEPTH_TEST)&&!glIsEnabled(GL_BLEND)&&!glIsEnabled(GL_CULL_FACE)&&!glIsEnabled(GL_STENCIL_TEST)&&!glIsEnabled(GL_POLYGON_OFFSET_FILL)&&viewport[2]==64&&viewport[3]==64);
 glEnable(0xdeadbeef);raster_calls=0;rc=draw(&r,&s,64,0xffff0000);CHECK("GL_error_invalidates_raster_snapshot",rc!=0&&!r.raster.valid);raster_calls=0;CHECK("GL_error_recovery_reapplies_raster",!draw(&r,&s,64,0xffff0000)&&reapplied()&&pixel(32,32,255,0,0));
 D8State masked=s;masked.rs[168]=1;if(draw(&other,&masked,64,0xff0000ff))return 2;raster_calls=0;CHECK("interleaved_renderer_reapplies_context_state",!draw(&r,&s,64,0xff00ff00)&&reapplied()&&pixel(32,32,0,255,0));
 printf("backend=%s overall=%s\n",r.gpu_program?"GPU":"CPU",failed?"FAIL":"PASS");d8_renderer_close(&other);d8_renderer_close(&r);SDL_GL_DeleteContext(gl);SDL_DestroyWindow(w);SDL_Quit();return failed;
}
