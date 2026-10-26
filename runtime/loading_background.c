#include "loading_background.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>
typedef void (GL_APIENTRYP CopyTexture)(GLenum,GLint,GLint,GLint,GLint,GLint,GLsizei,GLsizei);
typedef struct {
    GLint program,viewport[4],array,active,texture;
    GLint enabled,size,type,stride,normalized,attribute_buffer;
    void *pointer;
    GLboolean blend,depth,stencil,cull,scissor,alpha_coverage,sample_coverage,mask[4];
} LoadingState;
static void loading_cap(GLenum cap,GLboolean on){if(on)glEnable(cap);else glDisable(cap);}
static void loading_save(LoadingState *s) {
    glGetIntegerv(GL_CURRENT_PROGRAM,&s->program);glGetIntegerv(GL_VIEWPORT,s->viewport);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&s->array);glGetIntegerv(GL_ACTIVE_TEXTURE,&s->active);
    glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&s->texture);
    s->blend=glIsEnabled(GL_BLEND);s->depth=glIsEnabled(GL_DEPTH_TEST);s->stencil=glIsEnabled(GL_STENCIL_TEST);
    s->cull=glIsEnabled(GL_CULL_FACE);s->scissor=glIsEnabled(GL_SCISSOR_TEST);
    s->alpha_coverage=glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE);s->sample_coverage=glIsEnabled(GL_SAMPLE_COVERAGE);
    glGetBooleanv(GL_COLOR_WRITEMASK,s->mask);
    glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&s->enabled);
    glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_SIZE,&s->size);
    glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_TYPE,&s->type);
    glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_STRIDE,&s->stride);
    glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,&s->normalized);
    glGetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&s->attribute_buffer);
    glGetVertexAttribPointerv(0,GL_VERTEX_ATTRIB_ARRAY_POINTER,&s->pointer);
}
static void loading_restore_state(const LoadingState *s) {
    glBindBuffer(GL_ARRAY_BUFFER,(GLuint)s->attribute_buffer);
    glVertexAttribPointer(0,s->size,(GLenum)s->type,(GLboolean)s->normalized,s->stride,s->pointer);
    loading_cap(GL_BLEND,s->blend);loading_cap(GL_DEPTH_TEST,s->depth);loading_cap(GL_STENCIL_TEST,s->stencil);
    loading_cap(GL_CULL_FACE,s->cull);loading_cap(GL_SCISSOR_TEST,s->scissor);
    loading_cap(GL_SAMPLE_ALPHA_TO_COVERAGE,s->alpha_coverage);loading_cap(GL_SAMPLE_COVERAGE,s->sample_coverage);
    if(s->enabled)glEnableVertexAttribArray(0);else glDisableVertexAttribArray(0);
    glColorMask(s->mask[0],s->mask[1],s->mask[2],s->mask[3]);
    glBindBuffer(GL_ARRAY_BUFFER,(GLuint)s->array);glBindTexture(GL_TEXTURE_2D,(GLuint)s->texture);
    glActiveTexture((GLenum)s->active);glUseProgram((GLuint)s->program);
    glViewport(s->viewport[0],s->viewport[1],s->viewport[2],s->viewport[3]);
}
static GLuint loading_shader(GLenum kind,const char *src) {
    GLuint shader=glCreateShader(kind);glShaderSource(shader,1,&src,NULL);glCompileShader(shader);
    GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){glDeleteShader(shader);return 0;}return shader;
}
static int loading_program(DsLoadingBackground *b) {
    if(b->program)return 0;
    const char *vs="attribute vec4 p;varying highp vec2 uv;void main(){gl_Position=vec4(p.xy,0.,1.);uv=p.zw;}";
    const char *fs="precision mediump float;varying highp vec2 uv;uniform sampler2D image;void main(){gl_FragColor=texture2D(image,uv);}";
    GLuint v=loading_shader(GL_VERTEX_SHADER,vs),f=loading_shader(GL_FRAGMENT_SHADER,fs);
    if(!v||!f){if(v)glDeleteShader(v);if(f)glDeleteShader(f);return -1;}
    GLuint p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glBindAttribLocation(p,0,"p");glLinkProgram(p);
    glDeleteShader(v);glDeleteShader(f);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){glDeleteProgram(p);return -1;}b->program=p;
    glUseProgram(p);glUniform1i(glGetUniformLocation(p,"image"),0);
    const GLfloat quad[]={-1,-1,0,0, 1,-1,1,0, -1,1,0,1, 1,1,1,1};
    glGenBuffers(1,&b->buffer);glBindBuffer(GL_ARRAY_BUFFER,b->buffer);
    glBufferData(GL_ARRAY_BUFFER,sizeof quad,quad,GL_STATIC_DRAW);return 0;
}
int ds_loading_background_capture(DsLoadingBackground *b,int width,int height) {
    b->valid=0;
    GLint fbo=0;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&fbo);
    if(fbo||width<1||height<1||width>2048||height>2048)return -1;
    /* GLES2 core operation, resolved on the context-owning thread. It bypasses
     * the producer proxy only inside the explicit blocking context job. */
    CopyTexture copy=(CopyTexture)SDL_GL_GetProcAddress("glCopyTexSubImage2D");
    if(!copy)return -1;
    GLint active,texture;glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
    if(!b->texture)glGenTextures(1,&b->texture);glBindTexture(GL_TEXTURE_2D,b->texture);
    if(b->width!=width||b->height!=height) {
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
        b->width=width;b->height=height;
    }
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    copy(GL_TEXTURE_2D,0,0,0,0,0,width,height);
    GLenum error=glGetError();glBindTexture(GL_TEXTURE_2D,(GLuint)texture);glActiveTexture((GLenum)active);
    b->valid=error==GL_NO_ERROR;return b->valid?0:-1;
}
int ds_loading_background_restore(DsLoadingBackground *b,int width,int height) {
    if(!b->valid)return 0;
    if(width!=b->width||height!=b->height){b->valid=0;return 0;}
    GLint fbo=0;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&fbo);if(fbo)return -1;
    LoadingState old;loading_save(&old);int result=loading_program(b);
    if(!result) {
        glUseProgram(b->program);glViewport(0,0,width,height);
        glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);glDisable(GL_SAMPLE_COVERAGE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glBindTexture(GL_TEXTURE_2D,b->texture);glBindBuffer(GL_ARRAY_BUFFER,b->buffer);
        glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,4*sizeof(GLfloat),NULL);glEnableVertexAttribArray(0);
        glDrawArrays(GL_TRIANGLE_STRIP,0,4);if(glGetError()!=GL_NO_ERROR)result=-1;
    }
    loading_restore_state(&old);if(result)b->valid=0;return result;
}
void ds_loading_background_close(DsLoadingBackground *b) {
    glDeleteTextures(1,&b->texture);glDeleteBuffers(1,&b->buffer);if(b->program)glDeleteProgram(b->program);memset(b,0,sizeof *b);
}
