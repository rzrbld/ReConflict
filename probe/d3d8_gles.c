/* First fixed-function D3D8 -> GLES2 backend slice. External SDL2 owns context.
 * The byte/float packing is independent of Win32 and the host's pointer size.
 * Texture combiners, lighting, fog, shaders, render targets and guest COM
 * objects still require implementation before this can render the game. */
#include "d3d8_gles.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float pos[4], color[4], uv[2]; } Vertex;
static GLuint compile(GLenum type, const char *source) {
    GLuint s = glCreateShader(type); glShaderSource(s, 1, &source, NULL); glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[2048]; glGetShaderInfoLog(s, sizeof(log), NULL, log); fprintf(stderr,"D3D8_GLES_shader=%s\n",log); glDeleteShader(s); return 0; }
    return s;
}
int ds_gles_init(DS_Gles *r) {
    memset(r, 0, sizeof(*r));
    const char *vs =
        "attribute vec4 position; attribute vec4 color; attribute vec2 uv;"
        "uniform vec2 viewport; uniform bool transformed; uniform mat4 matrix;"
        "varying vec4 vcolor; varying vec2 vuv;"
        "void main(){vec4 p; if(transformed){"
        "vec2 xy=(position.xy+vec2(0.5))/viewport;"
        "p=vec4(xy.x*2.-1.,1.-xy.y*2.,position.z*2.-1.,1.);"
        "p/=position.w; }else{ p=matrix*vec4(position.xyz,1.); p.z=2.*p.z-p.w; }"
        "gl_Position=p; vcolor=color; vuv=uv;}";
    const char *fs =
        "precision mediump float; varying vec4 vcolor; varying vec2 vuv;"
        "uniform sampler2D image; uniform bool textured;"
        "void main(){gl_FragColor=vcolor*(textured?texture2D(image,vuv):vec4(1.));}";
    GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) { if(v)glDeleteShader(v); if(f)glDeleteShader(f); return -1; }
    r->program = glCreateProgram(); glAttachShader(r->program,v); glAttachShader(r->program,f);
    glBindAttribLocation(r->program,0,"position"); glBindAttribLocation(r->program,1,"color"); glBindAttribLocation(r->program,2,"uv");
    glLinkProgram(r->program); glDeleteShader(v); glDeleteShader(f);
    GLint ok; glGetProgramiv(r->program,GL_LINK_STATUS,&ok);
    if (!ok) { ds_gles_destroy(r); return -1; }
    r->viewport=glGetUniformLocation(r->program,"viewport"); r->matrix=glGetUniformLocation(r->program,"matrix");
    r->transformed=glGetUniformLocation(r->program,"transformed"); r->textured=glGetUniformLocation(r->program,"textured");
    glUseProgram(r->program); glUniform1i(glGetUniformLocation(r->program,"image"),0);
    glGenBuffers(1,&r->vbo); return glGetError() ? -1 : 0;
}
void ds_gles_destroy(DS_Gles *r) {
    glDeleteBuffers(1,&r->vbo); glDeleteProgram(r->program); free(r->staging); memset(r,0,sizeof(*r));
}
int ds_gles_draw_up(DS_Gles *r, unsigned fvf, unsigned primitive, unsigned count,
                    const void *data, size_t stride, int width, int height,
                    const float matrix[16], GLuint texture) {
    unsigned pos = fvf & 0x00e, texcount = (fvf >> 8) & 15;
    if (!r->program || !data || !count || width<=0 || height<=0 ||
        (pos!=2 && pos!=4) || texcount>1 || (fvf & ~0x14eu) || (pos==2 && !matrix)) return -1;
    if (primitive<4 || primitive>6 || count>100000) return -1;
    unsigned n = primitive==4 ? count*3 : count+2;
    size_t needed = (pos==4 ? 16 : 12) + ((fvf & 0x40) ? 4 : 0) + (texcount ? 8 : 0);
    if (stride<needed || stride>4096) return -1;
    size_t bytes = sizeof(Vertex)*n;
    if (r->capacity<bytes) { void *p=realloc(r->staging,bytes); if(!p)return -1; r->staging=p; r->capacity=bytes; }
    Vertex *out = r->staging;
    for(unsigned i=0;i<n;++i) {
        const unsigned char *src=(const unsigned char *)data+i*stride;
        memcpy(out[i].pos,src,pos==4?16:12); src+=pos==4?16:12;
        if(pos==2)out[i].pos[3]=1;
        if(pos==4 && (!isfinite(out[i].pos[3]) || out[i].pos[3]<=0))return -1;
        uint32_t argb=UINT32_MAX;
        if(fvf&0x40){memcpy(&argb,src,4);src+=4;}
        out[i].color[0]=((argb>>16)&255)/255.f; out[i].color[1]=((argb>>8)&255)/255.f;
        out[i].color[2]=(argb&255)/255.f; out[i].color[3]=(argb>>24)/255.f;
        out[i].uv[0]=out[i].uv[1]=0;
        if(texcount)memcpy(out[i].uv,src,8);
    }
    static const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    glViewport(0,0,width,height); glUseProgram(r->program);
    glUniform2f(r->viewport,(float)width,(float)height); glUniform1i(r->transformed,pos==4);
    glUniformMatrix4fv(r->matrix,1,GL_FALSE,matrix?matrix:identity); glUniform1i(r->textured,texture!=0);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,texture);
    glBindBuffer(GL_ARRAY_BUFFER,r->vbo); glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)bytes,out,GL_STREAM_DRAW);
    glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void *)offsetof(Vertex,pos));
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void *)offsetof(Vertex,color));
    glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void *)offsetof(Vertex,uv));
    for(int i=0;i<3;++i)glEnableVertexAttribArray((GLuint)i);
    glDrawArrays(primitive==4?GL_TRIANGLES:primitive==5?GL_TRIANGLE_STRIP:GL_TRIANGLE_FAN,0,(GLsizei)n);
    return glGetError() ? -1 : 0;
}
GLuint ds_gles_texture(unsigned format,unsigned width,unsigned height,const void *data,size_t pitch) {
    unsigned bpp = format==21 || format==22 ? 4 : format==23 ? 2 : 0;
    if(!bpp || !data || !width || !height || width>4096 || height>4096 || pitch<width*bpp)return 0;
    unsigned char *rgba=malloc((size_t)width*height*4); if(!rgba)return 0;
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
        const unsigned char *p=(const unsigned char *)data+y*pitch+x*bpp;
        unsigned char *dst=rgba+((size_t)y*width+x)*4;
        if(bpp==4){dst[0]=p[2];dst[1]=p[1];dst[2]=p[0];dst[3]=format==21?p[3]:255;}
        else {uint16_t v;memcpy(&v,p,2);dst[0]=(unsigned char)(((v>>11)&31)*255/31);dst[1]=(unsigned char)(((v>>5)&63)*255/63);dst[2]=(unsigned char)((v&31)*255/31);dst[3]=255;}
    }
    GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,(GLsizei)width,(GLsizei)height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);free(rgba);
    if(glGetError()){glDeleteTextures(1,&texture);return 0;}return texture;
}
