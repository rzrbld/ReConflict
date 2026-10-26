#define _POSIX_C_SOURCE 200809L
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "d3d8_renderer.h"
#define SIDE 64
static int egl_error(const char *where){fprintf(stderr,"[egl-light] %s EGL error=%#x\n",where,eglGetError());return 2;}
static void value(GLuint program,const char *name){GLint loc=glGetUniformLocation(program,name);float v[4]={0};if(loc>=0)glGetUniformfv(program,loc,v);printf("uniform %s location=%d values=%.9g %.9g %.9g %.9g\n",name,loc,v[0],v[1],v[2],v[3]);}
static unsigned char image[2][SIDE*SIDE*4];
int main(void){
 EGLDisplay display=eglGetDisplay(EGL_DEFAULT_DISPLAY);if(display==EGL_NO_DISPLAY)return egl_error("get display");
 EGLint major=0,minor=0;if(!eglInitialize(display,&major,&minor))return egl_error("initialize");
 printf("EGL=%d.%d vendor=%s version=%s\n",major,minor,eglQueryString(display,EGL_VENDOR),eglQueryString(display,EGL_VERSION));
 EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
 EGLConfig config;EGLint configs=0;if(!eglChooseConfig(display,attrs,&config,1,&configs)||!configs)return egl_error("choose pbuffer config");
 if(!eglBindAPI(EGL_OPENGL_ES_API))return egl_error("bind ES API");EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};EGLContext context=eglCreateContext(display,config,EGL_NO_CONTEXT,ca);if(context==EGL_NO_CONTEXT)return egl_error("create context");
 EGLint pa[]={EGL_WIDTH,SIDE,EGL_HEIGHT,SIDE,EGL_NONE};EGLSurface surface=eglCreatePbufferSurface(display,config,pa);if(surface==EGL_NO_SURFACE)return egl_error("create pbuffer");
 if(!eglMakeCurrent(display,surface,surface,context))return egl_error("make pbuffer current");
 GLint limit;glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS,&limit);printf("GL=%s renderer=%s uniform_vectors=%d\n",glGetString(GL_VERSION),glGetString(GL_RENDERER),limit);
 D8Renderer r[2];unsetenv("DS_GPU_VERTEX");if(d8_renderer_init(r))return 2;setenv("DS_GPU_VERTEX","1",1);if(d8_renderer_init(r+1)||!r[1].gpu_program){fprintf(stderr,"GPU shader initialization failed\n");return 2;}
 D8State original;d8_state_defaults(&original,SIDE,SIDE);original.fvf=0x152;original.rs[7]=0;original.rs[22]=1;original.rs[137]=1;original.rs[29]=1;original.rs[139]=0xff000000;original.rs[142]=0;original.rs[143]=0;original.rs[145]=original.rs[147]=1;
 for(int j=4;j<8;j++)original.material[j]=1;for(int j=8;j<11;j++)original.material[j]=.045f;original.material[11]=1;original.material[15]=1;original.material[16]=25;
 original.transform[256][12]=-11933.7002f;original.transform[256][13]=139.600006f;original.transform[256][14]=-39408.3984f;
 /* Preserve actual world-space lighting, but flatten the sampled YZ triangle
  * into the pbuffer viewport. No window surface, native window, or swap call. */
 memset(original.transform[2],0,64);original.transform[2][8]=.055f;original.transform[2][5]=.055f;original.transform[2][12]=38440.f*.055f;original.transform[2][13]=-34.f*.055f;original.transform[2][14]=.5f;original.transform[2][15]=1;
 const float l4[]={0,1,.984314024f,.901961029f,1,1,1,1,1,.262744993f,.215685993f,.160784006f,1,-8154.78027f,552.017029f,-39077.8867f,0,0,0,4000,1,1,0,3.12499992e-7f,0,0};
 const float l5[]={0,1,.984314024f,.901961029f,1,1,1,1,1,.262744993f,.215685993f,.160784006f,1,-10111.001f,610.916992f,-39074.2109f,0,0,0,2000,1,1,0,1.24999997e-6f,0,0};
 memcpy(original.light[4],l4,sizeof l4);memcpy(original.light[5],l5,sizeof l5);uint32_t type=1;memcpy(original.light[4],&type,4);memcpy(original.light[5],&type,4);original.light_on[4]=original.light_on[5]=1;
 const uint32_t words[][9]={{0x410b3333,0xc2b96666,0x446ef99a,0x3f800000,0,0xb6c9539c,0xffffffff,0x40dd033c,0x3f54ea4b},{0x410b3333,0xc2b96666,0x4474d99a,0x3f800000,0,0xb6c9539c,0xffffffff,0x40e015f4,0x3f54ea4b},{0x410b3333,0xc2eb999a,0x446ef99a,0x3f800000,0x80000000,0xb58637bd,0xffffffff,0x40dd033c,0x3f6be469}};
 const GLuint textures[2]={0};int failures=0;
 for(int test=0;test<4;test++){D8State s=original;if(test==1){memcpy(s.light[0],s.light[4],sizeof s.light[0]);memcpy(s.light[1],s.light[5],sizeof s.light[1]);s.light_on[0]=s.light_on[1]=1;s.light_on[4]=s.light_on[5]=0;}if(test==2){s.light_on[4]=0;}if(test==3)s.rs[137]=0;
  for(int k=0;k<2;k++){glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);if(d8_renderer_draw(r+k,&s,SIDE,4,1,words,sizeof words[0],3,NULL,0,0,textures)){fprintf(stderr,"test %d backend %d: %s\n",test,k,d8_renderer_error());return 2;}glReadPixels(0,0,SIDE,SIDE,GL_RGBA,GL_UNSIGNED_BYTE,image[k]);}
  unsigned bad=0,maxdiff=0;for(unsigned i=0;i<sizeof image[0];i++){unsigned d=abs((int)image[0][i]-image[1][i]);if(d>maxdiff)maxdiff=d;if(d>3)bad++;}
  unsigned offset=(34*SIDE+22)*4;printf("case=%d cpu=%u,%u,%u gpu=%u,%u,%u maxdiff=%u channels_gt3=%u\n",test,image[0][offset],image[0][offset+1],image[0][offset+2],image[1][offset],image[1][offset+1],image[1][offset+2],maxdiff,bad);
  if(bad>12||image[0][offset]<5||image[1][offset]<5)failures++;
  value(r[1].gpu_program,"flags");value(r[1].gpu_program,"enabled[0]");value(r[1].gpu_program,"enabled[1]");value(r[1].gpu_program,"lights[28]");value(r[1].gpu_program,"lights[31]");value(r[1].gpu_program,"lights[33]");
 }
 /* Actual scope overlay: preserve diffuse alpha when COLORARG1 selects a
  * null texture, rather than drawing an opaque orange fullscreen quad. */
 D8State tint;d8_state_defaults(&tint,SIDE,SIDE);tint.fvf=0x144;tint.rs[7]=0;tint.rs[22]=1;tint.rs[27]=1;tint.rs[19]=5;tint.rs[20]=6;
 const struct {float p[4];uint32_t color;float uv[2];} quad[]={{{0,0,0,1},0x40fca90b,{0,0}},{{SIDE,0,0,1},0x40fca90b,{0,0}},{{SIDE,SIDE,0,1},0x40fca90b,{0,0}},{{0,SIDE,0,1},0x40fca90b,{0,0}}};
 for(int k=0;k<2;k++){glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);if(d8_renderer_draw(r+k,&tint,SIDE,6,2,quad,sizeof quad[0],4,NULL,0,0,textures))return 2;unsigned char pixel[4]={0};glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);int pass=pixel[0]>=62&&pixel[0]<=64&&pixel[1]>=41&&pixel[1]<=43&&pixel[2]>=193&&pixel[2]<=195;printf("scope_tint backend=%s pixel=%u,%u,%u expected=63,42,194 result=%s\n",k?"GPU":"CPU",pixel[0],pixel[1],pixel[2],pass?"PASS":"FAIL");if(!pass)failures++;}
 printf("result=%s failures=%d\n",failures?"FAIL":"PASS",failures);d8_renderer_close(r);d8_renderer_close(r+1);eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroySurface(display,surface);eglDestroyContext(display,context);eglTerminate(display);return failures?1:0;
}
