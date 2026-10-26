/* Context-ownership feasibility probe only: no game or command queue. */
#include <SDL.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef struct {
 SDL_Window *window;SDL_GLContext context;int width,height;SDL_atomic_t done;
 unsigned frames;int result;
} Probe;
static GLuint shader(GLenum kind,const char *text){
 GLuint s=glCreateShader(kind);glShaderSource(s,1,&text,NULL);glCompileShader(s);
 GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
 if(!ok){char log[1024];glGetShaderInfoLog(s,sizeof log,NULL,log);fprintf(stderr,"shader failed: %s\n",log);glDeleteShader(s);return 0;}return s;
}
static int render(void *arg){
 Probe *p=arg;p->result=1;
 if(SDL_GL_MakeCurrent(p->window,p->context)){fprintf(stderr,"worker MakeCurrent failed: %s\n",SDL_GetError());goto done;}
 if(SDL_GL_GetCurrentContext()!=p->context){fprintf(stderr,"worker context identity mismatch\n");goto detach;}
 printf("worker owns context thread=%lu GL=%s renderer=%s\n",(unsigned long)SDL_ThreadID(),glGetString(GL_VERSION),glGetString(GL_RENDERER));
 int interval=SDL_GL_SetSwapInterval(0);printf("worker SetSwapInterval(0)=%d actual=%d\n",interval,SDL_GL_GetSwapInterval());
 GLuint v=shader(GL_VERTEX_SHADER,"attribute vec2 p;void main(){gl_Position=vec4(p,0.,1.);}");
 GLuint f=shader(GL_FRAGMENT_SHADER,"precision mediump float;uniform vec4 color;void main(){gl_FragColor=color;}");
 GLuint program=0,buffer=0;
 if(!v||!f)goto cleanup;
 program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glBindAttribLocation(program,0,"p");glLinkProgram(program);
 GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);if(!linked){fprintf(stderr,"worker program link failed\n");goto cleanup;}
 const GLfloat triangle[]={-1,-1,3,-1,-1,3};
 glGenBuffers(1,&buffer);glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferData(GL_ARRAY_BUFFER,sizeof triangle,triangle,GL_STATIC_DRAW);
 glUseProgram(program);GLint color=glGetUniformLocation(program,"color");glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,0);glEnableVertexAttribArray(0);
 glViewport(0,0,p->width,p->height);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);
 for(unsigned n=0;n<8;n++){
  GLfloat c[4]={0,0,0,1};c[n%3]=1;glUniform4fv(color,1,c);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLES,0,3);
  GLint framebuffer=-1,viewport[4];glGetIntegerv(GL_FRAMEBUFFER_BINDING,&framebuffer);glGetIntegerv(GL_VIEWPORT,viewport);
  GLubyte pixel[4];glReadPixels(p->width/2,p->height/2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
  GLenum error=glGetError();int ok=framebuffer==0&&viewport[2]==p->width&&viewport[3]==p->height&&!error;
  for(unsigned k=0;k<4;k++)ok&=abs((int)pixel[k]-(c[k]?255:0))<=2;
  if(!ok){fprintf(stderr,"worker frame%u FAIL fbo=%d viewport=%dx%d GLerr=%#x pixel=%u,%u,%u,%u\n",n,framebuffer,viewport[2],viewport[3],error,pixel[0],pixel[1],pixel[2],pixel[3]);goto cleanup;}
  SDL_ClearError();SDL_GL_SwapWindow(p->window);
  const char *swap_error=SDL_GetError();error=glGetError();
  if((swap_error&&*swap_error)||error){fprintf(stderr,"worker SwapWindow failed SDL=%s GL=%#x\n",swap_error,error);goto cleanup;}
  p->frames++;SDL_Delay(16);
 }
 printf("worker draw/query/readback/SwapWindow frames=%u pixel_checks=PASS\n",p->frames);p->result=0;
cleanup:
 if(buffer)glDeleteBuffers(1,&buffer);
 if(program)glDeleteProgram(program);
 if(v)glDeleteShader(v);
 if(f)glDeleteShader(f);
detach:
 if(SDL_GL_MakeCurrent(NULL,NULL)){fprintf(stderr,"worker detach failed: %s\n",SDL_GetError());p->result=1;}
 else if(SDL_GL_GetCurrentContext()!=NULL){fprintf(stderr,"worker detach identity mismatch\n");p->result=1;}
done:
 SDL_AtomicSet(&p->done,1);return p->result;
}
int main(void){
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)){fprintf(stderr,"SDL init: %s\n",SDL_GetError());return 2;}
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
 Probe p;memset(&p,0,sizeof p);p.result=1;
 p.window=SDL_CreateWindow("render thread context probe",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,480,SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN_DESKTOP);
 if(!p.window){fprintf(stderr,"window: %s\n",SDL_GetError());SDL_Quit();return 2;}
 p.context=SDL_GL_CreateContext(p.window);if(!p.context){fprintf(stderr,"context: %s\n",SDL_GetError());SDL_DestroyWindow(p.window);SDL_Quit();return 2;}
 SDL_GL_GetDrawableSize(p.window,&p.width,&p.height);
 printf("main created context thread=%lu driver=%s drawable=%dx%d\n",(unsigned long)SDL_ThreadID(),SDL_GetCurrentVideoDriver(),p.width,p.height);
 if(SDL_GL_MakeCurrent(NULL,NULL)||SDL_GL_GetCurrentContext()!=NULL){fprintf(stderr,"main detach failed: %s\n",SDL_GetError());goto finish;}
 SDL_Thread *thread=SDL_CreateThread(render,"GL-probe",&p);
 if(!thread){fprintf(stderr,"thread: %s\n",SDL_GetError());goto finish;}
 unsigned pumps=0;int main_stayed_detached=1;
 while(!SDL_AtomicGet(&p.done)){
  SDL_Event event;while(SDL_PollEvent(&event)){}pumps++;
  main_stayed_detached&=SDL_GL_GetCurrentContext()==NULL;SDL_Delay(1);
 }
 int result;SDL_WaitThread(thread,&result);if(result||!main_stayed_detached)p.result=1;
 printf("main event pumps=%u stayed_detached=%s worker_join=%d\n",pumps,main_stayed_detached?"PASS":"FAIL",result);
 if(SDL_GL_MakeCurrent(p.window,p.context)||SDL_GL_GetCurrentContext()!=p.context){fprintf(stderr,"main reacquire failed: %s\n",SDL_GetError());p.result=1;}
 else printf("main reacquire after worker release=PASS\n");
finish:
 SDL_GL_MakeCurrent(NULL,NULL);SDL_GL_DeleteContext(p.context);SDL_DestroyWindow(p.window);SDL_Quit();
 printf("context_migration_probe=%s (not a game/performance test)\n",p.result?"FAIL":"PASS");return p.result;
}
