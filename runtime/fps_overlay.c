/* A default-on diagnostic HUD using actual completed Present intervals.
 * One small bitmap atlas and one draw. All touched game GL state is restored.
 * No SDL_ttf dependency and no draws when DS_FPS_OVERLAY=0. */
#include "fps_overlay.h"
#include <GLES2/gl2.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { GLfloat xy[2],uv[2];GLubyte color[4]; } Vertex;
typedef struct { uint64_t end,interval; } Interval;
static Interval intervals[4096];
static size_t first,count;
static uint64_t total,previous,frequency;
static int enabled=-1,initialized,failed;
static GLuint program,texture,buffer;
static GLint inverse_size;
static double rolling_fps;

static int overlay_enabled(void) {
    if(enabled<0){const char *s=SDL_getenv("DS_FPS_OVERLAY");enabled=!s||strcmp(s,"0");}
    return enabled;
}

void ds_fps_overlay_presented(void) {
    if(!overlay_enabled())return;
    if(!frequency)frequency=SDL_GetPerformanceFrequency();
    uint64_t now=SDL_GetPerformanceCounter();
    if(previous&&now>previous){
        if(count==4096){total-=intervals[first].interval;first=(first+1)%4096;count--;}
        intervals[(first+count)%4096]=(Interval){now,now-previous};
        total+=now-previous;count++;
        while(count>1&&now-intervals[first].end>=frequency){total-=intervals[first].interval;first=(first+1)%4096;count--;}
        rolling_fps=total?(double)count*(double)frequency/(double)total:0;
    }
    previous=now;
}

static GLuint compile(GLenum type,const char *source) {
    GLuint shader=glCreateShader(type);
    glShaderSource(shader,1,&source,NULL);glCompileShader(shader);
    GLint ok;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[1024];glGetShaderInfoLog(shader,sizeof log,NULL,log);fprintf(stderr,"[fps-overlay] shader failed: %s\n",log);glDeleteShader(shader);return 0;}
    return shader;
}

static int initialize(void) {
    const char *vs="attribute vec2 p;attribute vec2 uv;attribute vec4 color;uniform vec2 inverse_size;varying vec2 vuv;varying vec4 vc;void main(){gl_Position=vec4(p.x*inverse_size.x-1.,1.-p.y*inverse_size.y,0.,1.);vuv=uv;vc=color;}";
    const char *fs="precision mediump float;uniform sampler2D atlas;varying vec2 vuv;varying vec4 vc;void main(){gl_FragColor=vec4(vc.rgb,vc.a*texture2D(atlas,vuv).a);}";
    GLuint vertex=compile(GL_VERTEX_SHADER,vs),fragment=compile(GL_FRAGMENT_SHADER,fs);
    if(!vertex||!fragment){if(vertex)glDeleteShader(vertex);if(fragment)glDeleteShader(fragment);return -1;}
    program=glCreateProgram();glAttachShader(program,vertex);glAttachShader(program,fragment);
    glBindAttribLocation(program,0,"p");glBindAttribLocation(program,1,"uv");glBindAttribLocation(program,2,"color");
    glLinkProgram(program);glDeleteShader(vertex);glDeleteShader(fragment);
    GLint ok;glGetProgramiv(program,GL_LINK_STATUS,&ok);
    if(!ok){char log[1024];glGetProgramInfoLog(program,sizeof log,NULL,log);fprintf(stderr,"[fps-overlay] program failed: %s\n",log);return -1;}
    inverse_size=glGetUniformLocation(program,"inverse_size");
    glUseProgram(program);glUniform1i(glGetUniformLocation(program,"atlas"),0);
    /* Digits, F/P/S, period, hyphen and space: original 5x7 bitmaps. */
    static const uint8_t font[16][7]={
        {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
        {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
        {14,17,17,15,1,1,14},{31,16,16,30,16,16,16},{30,17,17,30,16,16,16},
        {15,16,16,14,1,1,30},{0,0,0,0,0,0,4},{0,0,0,31,0,0,0},{0,0,0,0,0,0,0}};
    uint8_t pixels[128*8*4];memset(pixels,0,sizeof pixels);
    for(unsigned glyph=0;glyph<16;glyph++)for(unsigned y=0;y<7;y++)for(unsigned x=0;x<5;x++)if(font[glyph][y]&(1u<<(4-x))){size_t p=((size_t)y*128+glyph*6+x)*4;pixels[p]=pixels[p+1]=pixels[p+2]=pixels[p+3]=255;}
    /* A single opaque texel is shared by the dark backing rectangle. */
    memset(pixels+(7*128+127)*4,255,4);
    glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,128,8,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    glGenBuffers(1,&buffer);
    GLenum error=glGetError();if(error){fprintf(stderr,"[fps-overlay] initialization GL error=%#x\n",error);return -1;}
    return 0;
}

typedef struct {
    GLint enabled,size,stride,type,normalized,buffer;
    void *pointer;
} Attribute;
typedef struct {
    GLint framebuffer,program,viewport[4],array_buffer,active_texture,texture0;
    GLint src_rgb,dst_rgb,src_alpha,dst_alpha,equation_rgb,equation_alpha;
    GLboolean blend,depth,stencil,cull,scissor,alpha_coverage,sample_coverage;
    GLboolean color_mask[4],depth_mask;
    Attribute attribute[3];
} SavedState;

static void save(SavedState *s) {
    glGetIntegerv(GL_FRAMEBUFFER_BINDING,&s->framebuffer);glGetIntegerv(GL_CURRENT_PROGRAM,&s->program);
    glGetIntegerv(GL_VIEWPORT,s->viewport);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&s->array_buffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&s->active_texture);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&s->texture0);
    glGetIntegerv(GL_BLEND_SRC_RGB,&s->src_rgb);glGetIntegerv(GL_BLEND_DST_RGB,&s->dst_rgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA,&s->src_alpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&s->dst_alpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB,&s->equation_rgb);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&s->equation_alpha);
    s->blend=glIsEnabled(GL_BLEND);s->depth=glIsEnabled(GL_DEPTH_TEST);s->stencil=glIsEnabled(GL_STENCIL_TEST);
    s->cull=glIsEnabled(GL_CULL_FACE);s->scissor=glIsEnabled(GL_SCISSOR_TEST);
    s->alpha_coverage=glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE);s->sample_coverage=glIsEnabled(GL_SAMPLE_COVERAGE);
    glGetBooleanv(GL_COLOR_WRITEMASK,s->color_mask);glGetBooleanv(GL_DEPTH_WRITEMASK,&s->depth_mask);
    for(GLuint i=0;i<3;i++){
        Attribute *a=s->attribute+i;
        glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&a->enabled);glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_SIZE,&a->size);
        glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_STRIDE,&a->stride);glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_TYPE,&a->type);
        glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,&a->normalized);glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&a->buffer);
        glGetVertexAttribPointerv(i,GL_VERTEX_ATTRIB_ARRAY_POINTER,&a->pointer);
    }
}

static void capability(GLenum cap,GLboolean value){if(value)glEnable(cap);else glDisable(cap);}
static void restore(const SavedState *s) {
    for(GLuint i=0;i<3;i++){
        const Attribute *a=s->attribute+i;
        glBindBuffer(GL_ARRAY_BUFFER,(GLuint)a->buffer);glVertexAttribPointer(i,a->size,(GLenum)a->type,(GLboolean)a->normalized,a->stride,a->pointer);
        if(a->enabled)glEnableVertexAttribArray(i);else glDisableVertexAttribArray(i);
    }
    glBindBuffer(GL_ARRAY_BUFFER,(GLuint)s->array_buffer);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,(GLuint)s->texture0);glActiveTexture((GLenum)s->active_texture);
    glUseProgram((GLuint)s->program);glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)s->framebuffer);
    glViewport(s->viewport[0],s->viewport[1],s->viewport[2],s->viewport[3]);
    glBlendFuncSeparate((GLenum)s->src_rgb,(GLenum)s->dst_rgb,(GLenum)s->src_alpha,(GLenum)s->dst_alpha);
    glBlendEquationSeparate((GLenum)s->equation_rgb,(GLenum)s->equation_alpha);
    capability(GL_BLEND,s->blend);capability(GL_DEPTH_TEST,s->depth);capability(GL_STENCIL_TEST,s->stencil);
    capability(GL_CULL_FACE,s->cull);capability(GL_SCISSOR_TEST,s->scissor);
    capability(GL_SAMPLE_ALPHA_TO_COVERAGE,s->alpha_coverage);capability(GL_SAMPLE_COVERAGE,s->sample_coverage);
    glColorMask(s->color_mask[0],s->color_mask[1],s->color_mask[2],s->color_mask[3]);glDepthMask(s->depth_mask);
}

static void quad(Vertex *out,float x,float y,float w,float h,float u0,float v0,float u1,float v1,const GLubyte color[4]) {
    const float xy[6][2]={{x,y},{x+w,y},{x,y+h},{x,y+h},{x+w,y},{x+w,y+h}};
    const float uv[6][2]={{u0,v0},{u1,v0},{u0,v1},{u0,v1},{u1,v0},{u1,v1}};
    for(int i=0;i<6;i++){memcpy(out[i].xy,xy[i],sizeof out[i].xy);memcpy(out[i].uv,uv[i],sizeof out[i].uv);memcpy(out[i].color,color,4);}
}

void ds_fps_overlay_draw(SDL_Window *window) {
    if(!overlay_enabled()||failed||!window)return;
    int width,height;SDL_GL_GetDrawableSize(window,&width,&height);if(width<1||height<1)return;
    SavedState old;save(&old);
    if(!initialized){if(initialize()){failed=1;restore(&old);return;}initialized=1;}
    char label[24];if(count)snprintf(label,sizeof label,"FPS %.1f",rolling_fps>999.9?999.9:rolling_fps);else snprintf(label,sizeof label,"FPS --.-");
    size_t length=strlen(label);if(length>15)length=15;
    Vertex vertices[96];const GLubyte backing[4]={5,8,10,208},white[4]={255,255,255,255};
    quad(vertices,6,6,(float)length*12+8,22,127.5f/128,7.5f/8,127.5f/128,7.5f/8,backing);
    for(size_t i=0;i<length;i++){
        int c=label[i],glyph=c>='0'&&c<='9'?c-'0':c=='F'?10:c=='P'?11:c=='S'?12:c=='.'?13:c=='-'?14:15;
        quad(vertices+(i+1)*6,10+(float)i*12,10,12,16,glyph*6.f/128,0,(glyph*6.f+6)/128,1,white);
    }
    glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,width,height);glUseProgram(program);
    glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glDisable(GL_STENCIL_TEST);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);glDisable(GL_SAMPLE_COVERAGE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glEnable(GL_BLEND);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);glUniform2f(inverse_size,2.f/width,2.f/height);
    glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferData(GL_ARRAY_BUFFER,(length+1)*6*sizeof(Vertex),vertices,GL_STREAM_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,xy));
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)offsetof(Vertex,uv));
    glVertexAttribPointer(2,4,GL_UNSIGNED_BYTE,GL_TRUE,sizeof(Vertex),(void*)offsetof(Vertex,color));
    for(GLuint i=0;i<3;i++)glEnableVertexAttribArray(i);
    glDrawArrays(GL_TRIANGLES,0,(GLsizei)((length+1)*6));
    restore(&old);
}

void ds_fps_overlay_close(void) {
    if(initialized||failed){glDeleteBuffers(1,&buffer);glDeleteTextures(1,&texture);glDeleteProgram(program);}
    buffer=texture=program=0;initialized=failed=0;first=count=0;total=previous=frequency=0;rolling_fps=0;
}
