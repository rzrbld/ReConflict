#include "loading_background.h"
#include "render_thread_proxy.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum {W=128,H=96};
static DsLoadingBackground background;
static unsigned char expected[W*H*4],actual[W*H*4];
static void test_job(void *arg) {
    (void)arg;
    GLuint other;glGenBuffers(1,&other);glBindBuffer(GL_ARRAY_BUFFER,other);
    float values[16]={0};glBufferData(GL_ARRAY_BUFFER,sizeof values,values,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,16,(void*)4);glEnableVertexAttribArray(0);
    for(int repeat=0;repeat<6;repeat++) {
        // Asymmetric pattern catches upside-down copies, borders and RGB loss.
        glColorMask(1,1,1,1);glEnable(GL_SCISSOR_TEST);
        for(int y=0;y<H;y+=8)for(int x=0;x<W;x+=8){glScissor(x,y,8,8);glClearColor((x+repeat)%128/127.f,y/95.f,((x+y+repeat*17)%128)/127.f,1.f);glClear(GL_COLOR_BUFFER_BIT);}
        glDisable(GL_SCISSOR_TEST);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,expected);
        assert(!ds_loading_background_capture(&background,W,H));assert(background.valid);
        for(int frame=0;frame<4;frame++) {
            glColorMask(1,1,1,1);glDisable(GL_SCISSOR_TEST);glClearColor(.68f,.74f,.8f,1);glClear(GL_COLOR_BUFFER_BIT);
            // Hostile state must neither affect the copy nor be lost afterward.
            glViewport(3,4,25,26);glEnable(GL_SCISSOR_TEST);glScissor(1,2,3,4);
            glEnable(GL_BLEND);glEnable(GL_DEPTH_TEST);glEnable(GL_STENCIL_TEST);glEnable(GL_CULL_FACE);
            glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);glEnable(GL_SAMPLE_COVERAGE);glColorMask(0,1,0,1);glActiveTexture(GL_TEXTURE1);
            assert(!ds_loading_background_restore(&background,W,H));
            GLint v[4],binding,size,stride,normalized,active;GLboolean mask[4];void *pointer;
            glGetIntegerv(GL_VIEWPORT,v);assert(v[0]==3&&v[1]==4&&v[2]==25&&v[3]==26);
            glGetIntegerv(GL_ACTIVE_TEXTURE,&active);assert(active==GL_TEXTURE1);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&binding);assert((GLuint)binding==other);
            glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_SIZE,&size);assert(size==3);
            glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_STRIDE,&stride);assert(stride==16);
            glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,&normalized);assert(!normalized);
            glGetVertexAttribPointerv(0,GL_VERTEX_ATTRIB_ARRAY_POINTER,&pointer);assert(pointer==(void*)4);
            glGetBooleanv(GL_COLOR_WRITEMASK,mask);assert(!mask[0]&&mask[1]&&!mask[2]&&mask[3]);
            assert(glIsEnabled(GL_BLEND)&&glIsEnabled(GL_DEPTH_TEST)&&glIsEnabled(GL_STENCIL_TEST)&&glIsEnabled(GL_CULL_FACE)&&glIsEnabled(GL_SCISSOR_TEST));
            assert(glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE)&&glIsEnabled(GL_SAMPLE_COVERAGE));
            glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,actual);assert(!memcmp(expected,actual,sizeof actual));
            // Draw a progress bar AFTER restore; it remains visible.
            glColorMask(1,1,1,1);glScissor(10,5,40,4);glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
            unsigned char pixel[4];glReadPixels(20,6,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);assert(pixel[0]==255&&!pixel[1]&&!pixel[2]);
            assert(glGetError()==GL_NO_ERROR);
        }
    }
    assert(!ds_loading_background_restore(&background,W+1,H));assert(!background.valid);
    assert(ds_loading_background_capture(&background,0,H)<0);assert(!background.valid);
    assert(!ds_loading_background_capture(&background,W,H));
    ds_loading_background_close(&background);assert(!background.valid&&!background.texture&&!background.program&&!background.buffer);
    assert(!ds_loading_background_restore(&background,W,H));
    // A fresh lifecycle must recreate every GL resource successfully.
    assert(!ds_loading_background_capture(&background,W,H));assert(!ds_loading_background_restore(&background,W,H));
    ds_loading_background_close(&background);glDeleteBuffers(1,&other);assert(glGetError()==GL_NO_ERROR);
}
int main(void) {
    assert(!SDL_Init(SDL_INIT_VIDEO));SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8);
    SDL_Window *w=SDL_CreateWindow("Loading retention test",0,0,W,H,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(w);
    SDL_GLContext ctx=SDL_GL_CreateContext(w);assert(ctx);printf("GL %s / %s\n",glGetString(GL_VERSION),glGetString(GL_RENDERER));
    assert(!ds_rt_start(w,ctx));assert(!ds_rt_call(test_job,NULL));assert(!ds_rt_stop());
    SDL_GL_DeleteContext(ctx);SDL_DestroyWindow(w);SDL_Quit();puts("Loading retention PASS: 24 exact pixel comparisons, bar composition, GL state, size invalidation, reset/recreate");return 0;
}
