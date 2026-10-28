#include <SDL.h>
#include <GLES2/gl2.h>
#include <assert.h>
#include <stdio.h>
typedef struct { SDL_Window *window; SDL_GLContext context; } Worker;
static int worker_test(void *opaque){
 Worker *w=opaque;assert(SDL_GL_MakeCurrent(w->window,w->context)==0);
 glClearColor(0,1,0,1);glClear(GL_COLOR_BUFFER_BIT);
 unsigned char pixel[4]={0};glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
 assert(glGetError()==GL_NO_ERROR);assert(pixel[0]<5&&pixel[1]>250&&pixel[2]<5);
 assert(SDL_GL_MakeCurrent(w->window,NULL)==0);return 0;
}
int main(void){
 assert(SDL_Init(SDL_INIT_VIDEO)==0);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
 SDL_Window *w=SDL_CreateWindow("GLES loader test",0,0,32,32,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
 assert(w);SDL_GLContext c=SDL_GL_CreateContext(w);assert(c);
 assert(glGetString(GL_VERSION));
 glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
 unsigned char pixel[4]={0};glReadPixels(0,0,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
 assert(glGetError()==GL_NO_ERROR);assert(pixel[0]>250&&pixel[1]<5&&pixel[2]<5);
 printf("PASS SDL GLES dispatch: %s; red pixel readback\n",glGetString(GL_RENDERER));
 assert(SDL_GL_MakeCurrent(w,NULL)==0);
 Worker worker={w,c};SDL_Thread *thread=SDL_CreateThread(worker_test,"gl-worker",&worker);assert(thread);
 int result;SDL_WaitThread(thread,&result);assert(result==0);
 puts("PASS context transfer to worker: green pixel readback");
 SDL_GL_DeleteContext(c);SDL_DestroyWindow(w);SDL_Quit();return 0;
}
