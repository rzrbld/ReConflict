#ifndef DS_RENDER_WORKER_FIXTURE_H
#define DS_RENDER_WORKER_FIXTURE_H
/* Test fixtures only; no queue implementation or production ownership policy. */
#include <SDL.h>
#include <GLES2/gl2.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef struct { SDL_Window *window;SDL_GLContext context; } RenderFixture;
static RenderFixture render_fixture_open(void){
    RenderFixture fixture={0};
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)==0);
    assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES)==0);
    assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2)==0);
    assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0)==0);
    fixture.window=SDL_CreateWindow("worker regression",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    if(!fixture.window){fprintf(stderr,"SDL window: %s\n",SDL_GetError());abort();}
    fixture.context=SDL_GL_CreateContext(fixture.window);
    if(!fixture.context){fprintf(stderr,"SDL context: %s\n",SDL_GetError());abort();}
    SDL_GL_SetSwapInterval(0);
    return fixture;
}
static void render_fixture_close(RenderFixture *fixture){
    SDL_GL_MakeCurrent(NULL,NULL);SDL_GL_DeleteContext(fixture->context);
    SDL_DestroyWindow(fixture->window);SDL_Quit();memset(fixture,0,sizeof *fixture);
}
typedef struct {SDL_mutex *mutex;SDL_cond *condition;int entered,released;} RenderTestGate;
static void render_gate_open(RenderTestGate *gate){
    memset(gate,0,sizeof *gate);gate->mutex=SDL_CreateMutex();gate->condition=SDL_CreateCond();
    assert(gate->mutex&&gate->condition);
}
static void render_gate_worker_wait(RenderTestGate *gate){
    assert(SDL_LockMutex(gate->mutex)==0);gate->entered=1;SDL_CondBroadcast(gate->condition);
    while(!gate->released)assert(SDL_CondWait(gate->condition,gate->mutex)==0);
    SDL_UnlockMutex(gate->mutex);
}
static void render_gate_wait_entered(RenderTestGate *gate){
    assert(SDL_LockMutex(gate->mutex)==0);
    while(!gate->entered){int result=SDL_CondWaitTimeout(gate->condition,gate->mutex,5000);assert(result==0);}
    SDL_UnlockMutex(gate->mutex);
}
static void render_gate_release(RenderTestGate *gate){
    assert(SDL_LockMutex(gate->mutex)==0);gate->released=1;SDL_CondBroadcast(gate->condition);SDL_UnlockMutex(gate->mutex);
}
static void render_gate_close(RenderTestGate *gate){SDL_DestroyCond(gate->condition);SDL_DestroyMutex(gate->mutex);}
#endif
