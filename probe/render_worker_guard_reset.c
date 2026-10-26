/* Independent behavioral test for staged authoritative client-state reset.
 * Uses only public API and actual GL queries, never proxy implementation globals. */
#include "render_thread_proxy.h"
#include "../tests/render_worker_fixture.h"
typedef struct {GLuint real_buffer;GLfloat client[6];GLint captured_buffer,enabled;void *pointer;} State;
static void disabled_deleted_offset(void *arg){
    State *s=arg;GLuint temporary=0;glGenBuffers(1,&temporary);assert(temporary);glBindBuffer(GL_ARRAY_BUFFER,temporary);
    GLfloat data[16]={0};glBufferData(GL_ARRAY_BUFFER,sizeof data,data,GL_STATIC_DRAW);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,(void*)8);glDisableVertexAttribArray(0);glDeleteBuffers(1,&temporary);
    glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&s->captured_buffer);glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&s->enabled);glGetVertexAttribPointerv(0,GL_VERTEX_ATTRIB_ARRAY_POINTER,&s->pointer);
    assert(glGetError()==GL_NO_ERROR);
}
static void disabled_actual_client(void *arg){State *s=arg;glBindBuffer(GL_ARRAY_BUFFER,0);glDisableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,s->client);assert(glGetError()==GL_NO_ERROR);}
static void authoritative_vbo(void *arg){State *s=arg;glBindBuffer(GL_ARRAY_BUFFER,s->real_buffer);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,0);glEnableVertexAttribArray(0);assert(glGetError()==GL_NO_ERROR);}
static void barrier(void *arg){RenderTestGate *gate;memcpy(&gate,arg,sizeof gate);render_gate_worker_wait(gate);}
typedef struct {RenderTestGate *gate;SDL_atomic_t returned;int expect_async;} Observe;
static int release_observed(void *arg){
    Observe *o=arg;SDL_Delay(200);int returned=SDL_AtomicGet(&o->returned);
    if(returned!=o->expect_async){fprintf(stderr,"Draw return before worker release=%d expected=%d\n",returned,o->expect_async);abort();}
    render_gate_release(o->gate);return 0;
}
static void observe_draw(int expect_async,GLenum expected_error,const char *label){
    RenderTestGate gate;render_gate_open(&gate);RenderTestGate *pointer=&gate;
    assert(ds_rt_submit(barrier,&pointer,sizeof pointer)==0);render_gate_wait_entered(&gate);
    Observe observed={.gate=&gate,.expect_async=expect_async};SDL_AtomicSet(&observed.returned,0);
    SDL_Thread *thread=SDL_CreateThread(release_observed,"observe-draw",&observed);assert(thread);
    glDrawArrays(GL_TRIANGLES,0,3);SDL_AtomicSet(&observed.returned,1);
    assert(glGetError()==expected_error);assert(glGetError()==GL_NO_ERROR);
    int status;SDL_WaitThread(thread,&status);assert(status==0);assert(ds_rt_sync()==0);render_gate_close(&gate);printf("PASS %s draw=%s driver_error=%#x\n",label,expect_async?"async":"borrowed-sync",expected_error);
}
int main(void){
    RenderFixture fixture=render_fixture_open();printf("guard-reset SDL=%s GL=%s renderer=%s\n",SDL_GetCurrentVideoDriver(),glGetString(GL_VERSION),glGetString(GL_RENDERER));
    assert(!setenv("DS_RENDER_THREAD","1",1));assert(ds_rt_start(fixture.window,fixture.context)==0&&ds_rt_active());
    State state={.client={-1,-1,3,-1,-1,3}};assert(ds_rt_call(disabled_deleted_offset,&state)==0);
    assert(state.captured_buffer==0&&!state.enabled&&state.pointer==(void*)8);
    glGenBuffers(1,&state.real_buffer);assert(state.real_buffer);glBindBuffer(GL_ARRAY_BUFFER,state.real_buffer);glBufferData(GL_ARRAY_BUFFER,sizeof state.client,state.client,GL_STATIC_DRAW);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,0);glEnableVertexAttribArray(0);assert(ds_rt_sync()==0);
    observe_draw(1,GL_NO_ERROR,"disabled deleted-VBO nonNULL offset clears stale history");
    assert(ds_rt_call(disabled_actual_client,&state)==0);glEnableVertexAttribArray(0);assert(ds_rt_sync()==0);
    observe_draw(0,GL_NO_ERROR,"later enable of actual client descriptor forces borrowing");
    glBindBuffer(GL_ARRAY_BUFFER,state.real_buffer);glVertexAttribPointer(0,5,GL_FLOAT,GL_FALSE,0,0);
    observe_draw(0,GL_INVALID_VALUE,"rejected setter remains conservative before authoritative reset");
    assert(ds_rt_call(authoritative_vbo,&state)==0);observe_draw(1,GL_NO_ERROR,"authoritative native VBO state restores async");
    glDisableVertexAttribArray(0);glDeleteBuffers(1,&state.real_buffer);assert(ds_rt_stop()==0);assert(SDL_GL_GetCurrentContext()==fixture.context);render_fixture_close(&fixture);
    puts("render_worker_guard_reset=PASS (behavioral correctness only; no game/performance claim)");return 0;
}
