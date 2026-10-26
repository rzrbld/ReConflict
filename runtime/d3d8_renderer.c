/* Native GLES2 fixed-function pipeline used by the guest D3D8 COM adapter.
 * The guest remains 32-bit; vertex records are decoded with memcpy rather
 * than casting arbitrary guest data to a host ABI structure. */
#include "d3d8_renderer.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
static char draw_error[320];
static D8Renderer *raster_owner;
static D8Renderer *attribute_owner,*texture_owner;
void d8_renderer_invalidate_attributes(D8Renderer *r){memset(&r->attributes,0,sizeof r->attributes);attribute_owner=NULL;}
static unsigned texture_slot(GLuint texture){return (texture*2654435761u)>>(32-10);}
static void next_texture_epoch(D8Renderer *r){if(++r->texture_epoch==0){memset(r->texture_parameters,0,sizeof r->texture_parameters);r->texture_epoch=1;}}
void d8_renderer_invalidate_texture_parameters(D8Renderer *r){next_texture_epoch(r);texture_owner=NULL;}
void d8_renderer_forget_texture(D8Renderer *r,GLuint texture){
    if(!texture)return;D8TextureParameters *p=&r->texture_parameters[texture_slot(texture)];if(p->texture==texture)p->valid=0;
    if(texture_owner!=r)texture_owner=NULL;
}
void d8_renderer_invalidate_raster(D8Renderer *r){r->raster.valid=0;raster_owner=NULL;}
const char *d8_renderer_error(void){return draw_error[0]?draw_error:"unspecified renderer rejection";}
static int reject(const char *fmt,...){va_list ap;va_start(ap,fmt);vsnprintf(draw_error,sizeof draw_error,fmt,ap);va_end(ap);return -1;}
typedef struct { float p[4], c[4], spec[4], uv[2][2], fog; } Vertex;
static float asfloat(uint32_t u) { float f; memcpy(&f,&u,4); return f; }
static void argb(uint32_t a,float c[4]) { c[0]=((a>>16)&255)/255.f;c[1]=((a>>8)&255)/255.f;c[2]=(a&255)/255.f;c[3]=(a>>24)/255.f; }
static void identity(float m[16]) { memset(m,0,64);m[0]=m[5]=m[10]=m[15]=1; }
void d8_matrix_multiply(float out[16], const float a[16],const float b[16]) {
    float tmp[16];for(int r=0;r<4;r++)for(int c=0;c<4;c++){tmp[r*4+c]=0;for(int k=0;k<4;k++)tmp[r*4+c]+=a[r*4+k]*b[k*4+c];}memcpy(out,tmp,64);
}
static void vector(float out[4], const float in[4], const float m[16]) {
    float tmp[4];for(int c=0;c<4;c++){tmp[c]=0;for(int k=0;k<4;k++)tmp[c]+=in[k]*m[k*4+c];}memcpy(out,tmp,16);
}
void d8_state_defaults(D8State *s,unsigned w,unsigned h) {
    memset(s,0,sizeof(*s));for(int i=0;i<512;i++)identity(s->transform[i]);
    s->rs[7]=1;s->rs[8]=3;s->rs[9]=2;s->rs[14]=1;s->rs[19]=2;s->rs[20]=1;s->rs[22]=3;s->rs[23]=4;
    s->rs[25]=8;s->rs[37]=0x3f800000;s->rs[38]=0x3f800000;s->rs[53]=s->rs[54]=s->rs[55]=1;s->rs[56]=8;
    s->rs[58]=s->rs[59]=UINT32_MAX;s->rs[60]=UINT32_MAX;s->rs[136]=s->rs[137]=s->rs[141]=1;s->rs[145]=1;s->rs[168]=15;s->rs[171]=1;
    for(int i=0;i<8;i++){s->tss[i][1]=i?1:4;s->tss[i][2]=2;s->tss[i][3]=1;s->tss[i][4]=i?1:2;s->tss[i][5]=2;s->tss[i][6]=1;
        s->tss[i][11]=(uint32_t)i;s->tss[i][13]=s->tss[i][14]=1;s->tss[i][16]=s->tss[i][17]=1;}
    s->viewport[2]=w;s->viewport[3]=h;s->viewport[5]=0x3f800000u;
    for(int i=0;i<4;i++)s->material[i]=1;
}
static GLuint shader(GLenum type,const char *src){GLuint o=glCreateShader(type);glShaderSource(o,1,&src,NULL);glCompileShader(o);GLint ok;glGetShaderiv(o,GL_COMPILE_STATUS,&ok);if(!ok){char msg[4096];glGetShaderInfoLog(o,sizeof(msg),NULL,msg);fprintf(stderr,"[d3d8] shader: %s\n",msg);glDeleteShader(o);return 0;}return o;}
/* Uniform locations belong to a linked program and remain valid until relink.
 * The overlay owns a different program, so it cannot invalidate these tables. */
static void cache_uniform_locations(GLuint program,D8UniformLocations *u){
    memset(u,0xff,sizeof(*u));
#define LOCATION(field,name) u->field=glGetUniformLocation(program,name)
    LOCATION(tex[0],"tex0");LOCATION(tex[1],"tex1");
    LOCATION(has[0],"has0");LOCATION(has[1],"has1");
    LOCATION(cop[0],"cop0");LOCATION(cop[1],"cop1");
    LOCATION(aop[0],"aop0");LOCATION(aop[1],"aop1");
#define NAMED(field) LOCATION(field,#field)
    NAMED(factor);NAMED(afunc);NAMED(aref);NAMED(fogmode);NAMED(fogcolor);NAMED(fogparam);NAMED(specular);
    NAMED(world);NAMED(view);NAMED(projection);NAMED(normalmat);NAMED(texmat0);NAMED(texmat1);NAMED(viewport);NAMED(zrange);
    NAMED(flags);NAMED(sources);NAMED(texflags);NAMED(material);NAMED(power);NAMED(ambient);NAMED(eye);NAMED(camera);NAMED(localviewer);NAMED(lights);NAMED(enabled);
#undef NAMED
#undef LOCATION
    glUseProgram(program);glUniform1i(u->tex[0],0);glUniform1i(u->tex[1],1);
}
/* Exact value caches belong to individual linked programs. Neither switching
 * programs nor drawing the overlay changes another program's uniforms. */
enum { SU_HAS0,SU_HAS1,SU_COP0,SU_COP1,SU_AOP0,SU_AOP1,SU_FACTOR,SU_FOGCOLOR,
       SU_AFUNC,SU_AREF,SU_FOGMODE,SU_FOGPARAM,SU_SPECULAR,SU_VIEWPORT,SU_ZRANGE,
       SU_FLAGS,SU_SOURCES,SU_TEXFLAGS,SU_LOCALVIEWER };
static int small_changed(D8SmallUniformCache *c,unsigned slot,const void *value,size_t size){
    uint32_t mask=1u<<slot;if((c->valid&mask)&&!memcmp(&c->value[slot],value,size))return 0;
    memcpy(&c->value[slot],value,size);c->valid|=mask;return 1;
}
static void small1i(D8SmallUniformCache *c,unsigned slot,GLint loc,GLint v){if(small_changed(c,slot,&v,sizeof v))glUniform1i(loc,v);}
static void small4i(D8SmallUniformCache *c,unsigned slot,GLint loc,GLint a,GLint b,GLint d,GLint e){GLint v[]={a,b,d,e};if(small_changed(c,slot,v,sizeof v))glUniform4i(loc,a,b,d,e);}
static void small1f(D8SmallUniformCache *c,unsigned slot,GLint loc,GLfloat v){if(small_changed(c,slot,&v,sizeof v))glUniform1f(loc,v);}
static void small2f(D8SmallUniformCache *c,unsigned slot,GLint loc,GLfloat a,GLfloat b){GLfloat v[]={a,b};if(small_changed(c,slot,v,sizeof v))glUniform2f(loc,a,b);}
static void small3f(D8SmallUniformCache *c,unsigned slot,GLint loc,GLfloat a,GLfloat b,GLfloat d){GLfloat v[]={a,b,d};if(small_changed(c,slot,v,sizeof v))glUniform3f(loc,a,b,d);}
static void small4f(D8SmallUniformCache *c,unsigned slot,GLint loc,GLfloat a,GLfloat b,GLfloat d,GLfloat e){GLfloat v[]={a,b,d,e};if(small_changed(c,slot,v,sizeof v))glUniform4f(loc,a,b,d,e);}
static void small4fv(D8SmallUniformCache *c,unsigned slot,GLint loc,const GLfloat v[4]){if(small_changed(c,slot,v,4*sizeof(*v)))glUniform4fv(loc,1,v);}
static GLuint make_gpu_program(GLuint fragment);
int d8_renderer_init(D8Renderer *r) {
    memset(r,0,sizeof(*r));const char *trim=getenv("DS_GPU_TRIM_VB");r->trim_vertex_prefix=trim&&atoi(trim);
    const char *vs="attribute vec4 p;attribute vec4 c;attribute vec4 spec;attribute vec2 uv0;attribute vec2 uv1;attribute float fog;varying vec4 dc;varying vec4 sc;varying vec2 tc0;varying vec2 tc1;varying float fd;void main(){gl_Position=p;dc=c;sc=spec;tc0=uv0;tc1=uv1;fd=fog;}";
    const char *fs=
    "precision mediump float;varying vec4 dc;varying vec4 sc;varying vec2 tc0;varying vec2 tc1;varying float fd;"
    "uniform sampler2D tex0;uniform sampler2D tex1;uniform bool has0;uniform bool has1;uniform ivec4 cop0;uniform ivec4 aop0;uniform ivec4 cop1;uniform ivec4 aop1;uniform vec4 factor;uniform int afunc;uniform float aref;uniform int fogmode;uniform vec4 fogcolor;uniform vec3 fogparam;uniform bool specular;"
    "vec4 arg(int n,vec4 t,vec4 cur){int code=n;bool comp=false;bool rep=false;if(code>=32){rep=true;code-=32;}if(code>=16){comp=true;code-=16;}vec4 v=dc;if(code==1)v=cur;else if(code==2)v=t;else if(code==3)v=factor;else if(code==4)v=sc;if(comp)v=vec4(1.)-v;if(rep)v=vec4(v.a);return v;}"
    "vec4 op(int n,vec4 a,vec4 b,vec4 t,vec4 cur){if(n==2)return a;if(n==3)return b;if(n==4)return a*b;if(n==5)return 2.*a*b;if(n==6)return 4.*a*b;if(n==7)return a+b;if(n==8)return a+b-.5;if(n==9)return 2.*(a+b-.5);if(n==10)return a-b;if(n==11)return a+b-a*b;if(n==12)return mix(b,a,dc.a);if(n==13)return mix(b,a,t.a);if(n==14)return mix(b,a,factor.a);if(n==15)return a+b*(1.-t.a);if(n==16)return mix(b,a,cur.a);if(n==24)return vec4(dot(a.rgb*2.-1.,b.rgb*2.-1.));return cur;}"
    "vec4 stage(ivec4 co,ivec4 ao,vec4 t,vec4 cur){if(co.x==1)return cur;vec4 o=op(co.x,arg(co.y,t,cur),arg(co.z,t,cur),t,cur);if(ao.x!=1)o.a=op(ao.x,arg(ao.y,t,cur),arg(ao.z,t,cur),t,cur).a;return clamp(o,0.,1.);}"
    "void main(){vec4 cur=dc;vec4 t0=has0?texture2D(tex0,tc0):vec4(1.);vec4 t1=has1?texture2D(tex1,tc1):vec4(1.);cur=stage(cop0,aop0,t0,cur);if(cop0.x!=1)cur=stage(cop1,aop1,t1,cur);if(specular)cur.rgb=clamp(cur.rgb+sc.rgb,0.,1.);"
    "if(afunc==1)discard;if(afunc==2&&!(cur.a<aref))discard;if(afunc==3&&!(abs(cur.a-aref)<.001))discard;if(afunc==4&&!(cur.a<=aref))discard;if(afunc==5&&!(cur.a>aref))discard;if(afunc==6&&abs(cur.a-aref)<.001)discard;if(afunc==7&&!(cur.a>=aref))discard;"
    "float f=1.;if(fogmode==4)f=fd;else if(fogmode==1)f=exp(-fogparam.z*fd);else if(fogmode==2)f=exp(-pow(fogparam.z*fd,2.));else if(fogmode==3)f=(fogparam.y-fd)/max(.0001,fogparam.y-fogparam.x);cur.rgb=mix(fogcolor.rgb,cur.rgb,clamp(f,0.,1.));gl_FragColor=cur;}";
    GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);if(!v||!f)return -1;
    r->program=glCreateProgram();glAttachShader(r->program,v);glAttachShader(r->program,f);
    const char *attrs[]={"p","c","spec","uv0","uv1","fog"};for(int i=0;i<6;i++)glBindAttribLocation(r->program,(GLuint)i,attrs[i]);
    glLinkProgram(r->program);
    const char *gpu=getenv("DS_GPU_VERTEX");if(gpu&&atoi(gpu)){r->gpu_program=make_gpu_program(f);if(r->gpu_program){glGenBuffers(1,&r->gpu_vbo);glGenBuffers(1,&r->gpu_ibo);fprintf(stderr,"[d3d8] optional GPU vertex path enabled\n");}else fprintf(stderr,"[d3d8] GPU vertex shader unavailable; using CPU reference\n");}
    glDeleteShader(v);glDeleteShader(f);GLint ok;glGetProgramiv(r->program,GL_LINK_STATUS,&ok);if(!ok)return -1;
    glGenBuffers(1,&r->vbo);if(r->gpu_program)cache_uniform_locations(r->gpu_program,&r->gpu_uniforms);cache_uniform_locations(r->program,&r->uniforms);return 0;
}
void d8_renderer_close(D8Renderer *r){if(raster_owner==r)raster_owner=NULL;if(attribute_owner==r)attribute_owner=NULL;if(texture_owner==r)texture_owner=NULL;glDeleteBuffers(1,&r->gpu_vbo);glDeleteBuffers(1,&r->gpu_ibo);glDeleteProgram(r->gpu_program);glDeleteBuffers(1,&r->vbo);glDeleteProgram(r->program);free(r->rebased_indices);free(r->vertices);memset(r,0,sizeof(*r));}
void d8_renderer_note_buffer_upload(D8Renderer *r,int index,size_t bytes){
    if(index){r->uploads.index_bytes+=bytes;r->uploads.index_uploads++;}
    else{r->uploads.vertex_bytes+=bytes;r->uploads.vertex_uploads++;}
}
D8UploadStats d8_renderer_take_upload_stats(D8Renderer *r){D8UploadStats value=r->uploads;memset(&r->uploads,0,sizeof r->uploads);return value;}
unsigned d8_primitive_vertices(unsigned t,unsigned n){if(t==1)return n;if(t==2)return n*2;if(t==3)return n+1;if(t==4)return n*3;if(t==5||t==6)return n+2;return 0;}
unsigned d8_fvf_stride(uint32_t f){unsigned p=f&14,n=(f>>8)&15;if(p!=2&&p!=4)return 0;if(n>8)return 0;unsigned z=p==4?16:12;if(f&16)z+=12;if(f&32)z+=4;if(f&64)z+=4;if(f&128)z+=4;for(unsigned i=0;i<n;i++){unsigned d=(f>>(16+2*i))&3;z+=(d==0?2:d==1?3:d==2?4:1)*4;}return z;}
static GLenum cmp(unsigned n){static const GLenum a[]={GL_ALWAYS,GL_NEVER,GL_LESS,GL_EQUAL,GL_LEQUAL,GL_GREATER,GL_NOTEQUAL,GL_GEQUAL,GL_ALWAYS};return n<9?a[n]:GL_ALWAYS;}
static GLenum blend(unsigned n){static const GLenum a[]={GL_ONE,GL_ZERO,GL_ONE,GL_SRC_COLOR,GL_ONE_MINUS_SRC_COLOR,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA,GL_DST_COLOR,GL_ONE_MINUS_DST_COLOR,GL_SRC_ALPHA_SATURATE};return n<12?a[n]:GL_ONE;}
static GLenum stencil(unsigned n){static const GLenum a[]={GL_KEEP,GL_KEEP,GL_ZERO,GL_REPLACE,GL_INCR,GL_DECR,GL_INVERT,GL_INCR_WRAP,GL_DECR_WRAP};return n<9?a[n]:GL_KEEP;}
static int raster_changed(D8RasterCache *c,unsigned bit,void *old,const void *key,size_t bytes){
    uint32_t mask=1u<<bit;if((c->valid&mask)&&!memcmp(old,key,bytes))return 0;
    memcpy(old,key,bytes);c->valid|=mask;return 1;
}
static void raster(D8Renderer *r,const D8State *s,unsigned target_height){
    D8RasterCache *c=&r->raster;
    /* Raster state belongs to the GL context, unlike program-owned uniforms.
     * Two renderer instances interleaving in one context must reapply state. */
    if(raster_owner!=r){c->valid=0;raster_owner=r;}
    uint32_t depth[]={s->rs[7],s->rs[14],s->rs[23]};
    if(raster_changed(c,0,c->depth,depth,sizeof depth)){
        if(s->rs[7])glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
        glDepthMask(s->rs[14]!=0);glDepthFunc(cmp(s->rs[23]));
    }
    uint32_t blending[]={s->rs[27],s->rs[19],s->rs[20],s->rs[171]};
    if(raster_changed(c,1,c->blend,blending,sizeof blending)){
        if(s->rs[27])glEnable(GL_BLEND);else glDisable(GL_BLEND);
        glBlendFunc(blend(s->rs[19]),blend(s->rs[20]));
        glBlendEquation(s->rs[171]==2?GL_FUNC_SUBTRACT:s->rs[171]==3?GL_FUNC_REVERSE_SUBTRACT:GL_FUNC_ADD);
    }
    if(raster_changed(c,2,&c->cull,&s->rs[22],4)){
        if(s->rs[22]==1)glDisable(GL_CULL_FACE);else{glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(s->rs[22]==2?GL_CCW:GL_CW);}
    }
    uint32_t stencil_state[]={s->rs[52],s->rs[56],s->rs[57],s->rs[58],s->rs[53],s->rs[54],s->rs[55],s->rs[59]};
    if(raster_changed(c,3,c->stencil,stencil_state,sizeof stencil_state)){
        if(s->rs[52]){glEnable(GL_STENCIL_TEST);glStencilFunc(cmp(s->rs[56]),(GLint)s->rs[57],s->rs[58]);glStencilOp(stencil(s->rs[53]),stencil(s->rs[54]),stencil(s->rs[55]));glStencilMask(s->rs[59]);}else glDisable(GL_STENCIL_TEST);
    }
    if(raster_changed(c,4,&c->color,&s->rs[168],4))glColorMask((s->rs[168]&1)!=0,(s->rs[168]&2)!=0,(s->rs[168]&4)!=0,(s->rs[168]&8)!=0);
    if(raster_changed(c,5,&c->bias,&s->rs[47],4)){
        if(s->rs[47]){glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(0,-(float)s->rs[47]);}else glDisable(GL_POLYGON_OFFSET_FILL);
    }
    uint32_t viewport[]={s->viewport[0],s->viewport[1],s->viewport[2],s->viewport[3],target_height};
    if(raster_changed(c,6,c->viewport,viewport,sizeof viewport))glViewport(s->viewport[0],(GLint)target_height-(GLint)s->viewport[1]-(GLint)s->viewport[3],s->viewport[2],s->viewport[3]);
    if(raster_changed(c,7,c->depth_range,&s->viewport[4],8))glDepthRangef(asfloat(s->viewport[4]),asfloat(s->viewport[5]));
}
static void normalize(float v[3]){float n=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);if(n>0){v[0]/=n;v[1]/=n;v[2]/=n;}}
static int inverse3(const float m[16],float inv[9]) {
    float a=m[0],b=m[1],c=m[2],d=m[4],e=m[5],f=m[6],g=m[8],h=m[9],i=m[10];
    float det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if(fabsf(det)<1e-20f)return -1;
    inv[0]=(e*i-f*h)/det;inv[1]=(c*h-b*i)/det;inv[2]=(b*f-c*e)/det;
    inv[3]=(f*g-d*i)/det;inv[4]=(a*i-c*g)/det;inv[5]=(c*d-a*f)/det;
    inv[6]=(d*h-e*g)/det;inv[7]=(b*g-a*h)/det;inv[8]=(a*e-b*d)/det;return 0;
}
static const float *material_source(unsigned source,const float *material,const float *color,const float *spec,uint32_t fvf) {
    return source==1&&(fvf&64)?color:source==2&&(fvf&128)?spec:material;
}
static void light_vertex(const D8State *s,const float p[4],const float normal[3],float color[4],float specular[4]){
    float ambient[4],inverse[9],n[3]={normal[0],normal[1],normal[2]};argb(s->rs[139],ambient);
    if(!inverse3(s->transform[256],inverse))for(int j=0;j<3;j++)n[j]=normal[0]*inverse[j*3]+normal[1]*inverse[j*3+1]+normal[2]*inverse[j*3+2];
    if(s->rs[143])normalize(n);
    const float *diffuse=material_source(s->rs[141]?s->rs[145]:0,s->material,color,specular,s->fvf);
    const float *ma=material_source(s->rs[141]?s->rs[147]:0,s->material+4,color,specular,s->fvf);
    const float *ms=material_source(s->rs[141]?s->rs[146]:0,s->material+8,color,specular,s->fvf);
    const float *me=material_source(s->rs[141]?s->rs[148]:0,s->material+12,color,specular,s->fvf);
    float c[4],highlight[4]={0,0,0,specular[3]},eye[3]={-s->transform[2][2],-s->transform[2][6],-s->transform[2][10]};
    if(s->rs[142]&&!inverse3(s->transform[2],inverse))for(int j=0;j<3;j++)eye[j]=-(s->transform[2][12]*inverse[j]+s->transform[2][13]*inverse[3+j]+s->transform[2][14]*inverse[6+j])-p[j];
    normalize(eye);
    for(int k=0;k<3;k++)c[k]=me[k]+ambient[k]*ma[k];c[3]=diffuse[3];
    for(int i=0;i<8;i++)if(s->light_on[i]){
        const float *l=s->light[i];uint32_t type;memcpy(&type,l,4);float dir[3],att=1;
        if(type==3){dir[0]=-l[16];dir[1]=-l[17];dir[2]=-l[18];}
        else if(type==1||type==2){dir[0]=l[13]-p[0];dir[1]=l[14]-p[1];dir[2]=l[15]-p[2];float distance=sqrtf(dir[0]*dir[0]+dir[1]*dir[1]+dir[2]*dir[2]);if(distance>l[19])continue;att=1/fmaxf(.000001f,l[21]+l[22]*distance+l[23]*distance*distance);}
        else continue;
        normalize(dir);
        if(type==2){float spot[3]={l[16],l[17],l[18]};normalize(spot);float cosine=-(dir[0]*spot[0]+dir[1]*spot[1]+dir[2]*spot[2]),outer=cosf(l[25]*.5f),inner=cosf(l[24]*.5f);if(cosine<=outer)continue;if(cosine<inner)att*=powf((cosine-outer)/fmaxf(1e-8f,inner-outer),l[20]);}
        float dot=fmaxf(0,n[0]*dir[0]+n[1]*dir[1]+n[2]*dir[2]);
        for(int k=0;k<3;k++)c[k]+=att*(l[1+k]*diffuse[k]*dot+l[9+k]*ma[k]);
        if(s->rs[29]&&dot>0){float half[3]={dir[0]+eye[0],dir[1]+eye[1],dir[2]+eye[2]};normalize(half);float power=powf(fmaxf(0,n[0]*half[0]+n[1]*half[1]+n[2]*half[2]),s->material[16]);for(int k=0;k<3;k++)highlight[k]+=att*l[5+k]*ms[k]*power;}
    }
    for(int k=0;k<4;k++){color[k]=fminf(1,fmaxf(0,c[k]));specular[k]=fminf(1,fmaxf(0,highlight[k]));}
}
/* Opt-in native FVF path. Matrices are row-major D3D matrices: uploading with
 * transpose=false gives the column-vector transpose required by GLSL. */
static GLuint make_gpu_program(GLuint fragment){
 const char *src=
 "precision highp float;attribute vec4 p;attribute vec3 normal;attribute vec4 color;attribute vec4 spec;attribute vec4 uv0;attribute vec4 uv1;"
 "varying vec4 dc;varying vec4 sc;varying vec2 tc0;varying vec2 tc1;varying float fd;"
 "uniform mat4 world,view,projection,texmat0,texmat1;uniform mat3 normalmat;uniform vec4 viewport;uniform vec2 zrange;uniform vec3 eye,camera;uniform bool localviewer;"
 "uniform ivec4 flags;uniform ivec4 sources;uniform ivec4 texflags;uniform vec4 material[4];uniform float power;uniform vec4 ambient;"
 "uniform vec4 lights[56];uniform vec4 enabled[2];"
 "vec3 norm(vec3 a){float n=length(a);return n>0.?a/n:a;}"
 "vec4 source(int i,vec4 m){if(i==1)return color.bgra;if(i==2)return spec.bgra;return m;}"
 "vec2 coord(vec4 uv,mat4 m,int f,int flip){if(f!=0)uv=m*uv;float q=1.;if(f>=256){int n=f-256;if(n==1)q=uv.x;else if(n==2)q=uv.y;else if(n==3)q=uv.z;else q=uv.w;}if(q==0.)q=1.;vec2 t=uv.xy/q;if(flip!=0)t.y=1.-t.y;return t;}"
 "void main(){dc=color.bgra;sc=spec.bgra;tc0=coord(uv0,texmat0,texflags.x,texflags.z);tc1=coord(uv1,texmat1,texflags.y,texflags.w);"
 "if(flags.x!=0){float w=p.w!=0.?1./p.w:1.;vec2 xy=(p.xy+vec2(.5)-viewport.xy)/viewport.zw;float z=zrange.y!=zrange.x?(p.z-zrange.x)/(zrange.y-zrange.x):0.;gl_Position=vec4((xy.x*2.-1.)*w,(1.-xy.y*2.)*w,(2.*z-1.)*w,w);fd=flags.w!=0?p.z:sc.a;}"
 "else {vec4 wp=world*vec4(p.xyz,1.);vec4 vp=view*wp;gl_Position=projection*vp;gl_Position.z=2.*gl_Position.z-gl_Position.w;fd=abs(vp.z);"
 "if(flags.y!=0){vec3 n=normalmat*normal;if(flags.z!=0)n=norm(n);vec4 md=source(sources.x,material[0]);vec4 ma=source(sources.y,material[1]);vec4 ms=source(sources.z,material[2]);vec4 me=source(sources.w,material[3]);"
 "vec3 c=me.rgb+ambient.rgb*ma.rgb;vec3 highlight=vec3(0.);vec3 e=localviewer?norm(camera-wp.xyz):norm(eye);"
 "for(int i=0;i<8;i++){float on=0.;if(i<4){if(i==0)on=enabled[0].x;if(i==1)on=enabled[0].y;if(i==2)on=enabled[0].z;if(i==3)on=enabled[0].w;}else{if(i==4)on=enabled[1].x;if(i==5)on=enabled[1].y;if(i==6)on=enabled[1].z;if(i==7)on=enabled[1].w;}if(on==0.)continue;"
 "int j=i*7;vec4 a=lights[j];vec4 b=lights[j+1];vec4 c0=lights[j+2];vec4 d=lights[j+3];vec4 e0=lights[j+4];vec4 f=lights[j+5];vec4 g=lights[j+6];float type=a.x;vec3 l;float att=1.;"
 "if(type==3.)l=-e0.xyz;else if(type==1.||type==2.){l=d.yzw-wp.xyz;float distance=length(l);if(distance>e0.w)continue;att=1./max(.000001,f.y+f.z*distance+f.w*distance*distance);}else continue;l=norm(l);"
 "if(type==2.){float cosine=-dot(l,norm(e0.xyz));float outer=cos(g.y*.5),inner=cos(g.x*.5);if(cosine<=outer)continue;if(cosine<inner)att*=pow((cosine-outer)/max(.00000001,inner-outer),f.x);}"
 "float nd=max(0.,dot(n,l));c+=att*(a.yzw*md.rgb*nd+c0.yzw*ma.rgb);if(g.z!=0.&&nd>0.)highlight+=att*b.yzw*ms.rgb*pow(max(0.,dot(n,norm(l+e))),power);"
 "}dc=clamp(vec4(c,md.a),0.,1.);sc=clamp(vec4(highlight,sc.a),0.,1.);}}}";
 GLuint v=shader(GL_VERTEX_SHADER,src);if(!v)return 0;GLuint p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,fragment);
 const char *attrs[]={"p","normal","color","spec","uv0","uv1"};for(int i=0;i<6;i++)glBindAttribLocation(p,i,attrs[i]);glLinkProgram(p);glDeleteShader(v);GLint ok;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){char msg[2048];glGetProgramInfoLog(p,sizeof msg,NULL,msg);fprintf(stderr,"[d3d8] GPU vertex link: %s\n",msg);glDeleteProgram(p);return 0;}return p;
}
static void attributes_begin(D8Renderer *r){
    if(attribute_owner!=r){memset(&r->attributes,0,sizeof r->attributes);attribute_owner=r;}
}
static void gpu_attribute(D8Renderer *r,unsigned a,unsigned n,GLenum type,int normalized,size_t stride,size_t offset,GLuint buffer){
    D8AttributeCache *c=&r->attributes;uint32_t bit=1u<<a;
    if(!(c->enable_valid&bit)||!(c->enabled&bit)){glEnableVertexAttribArray(a);c->enable_valid|=bit;c->enabled|=bit;}
    /* Enabled-array draws may leave the generic current value undefined. */
    c->constant_valid&=~bit;
    if(!(c->pointer_valid&bit)||c->pointer[a].buffer!=buffer||c->pointer[a].size!=(GLint)n||c->pointer[a].type!=type||c->pointer[a].normalized!=(GLboolean)normalized||c->pointer[a].stride!=(GLsizei)stride||c->pointer[a].offset!=offset){
        glVertexAttribPointer(a,n,type,normalized,stride,(void*)offset);
        c->pointer[a].buffer=buffer;c->pointer[a].size=n;c->pointer[a].type=type;c->pointer[a].normalized=normalized;c->pointer[a].stride=stride;c->pointer[a].offset=offset;c->pointer_valid|=bit;
    }
}
static void gpu_constant(D8Renderer *r,unsigned a,float x,float y,float z,float w){
    D8AttributeCache *c=&r->attributes;uint32_t bit=1u<<a;float v[]={x,y,z,w};
    if(!(c->enable_valid&bit)||(c->enabled&bit)){glDisableVertexAttribArray(a);c->enable_valid|=bit;c->enabled&=~bit;}
    if(!(c->constant_valid&bit)||memcmp(c->constant[a],v,sizeof v)){glVertexAttrib4f(a,x,y,z,w);memcpy(c->constant[a],v,sizeof v);c->constant_valid|=bit;}
}
static void texture_parameters(D8Renderer *r,GLuint texture,const uint32_t tss[32]){
    if(texture_owner!=r){next_texture_epoch(r);texture_owner=r;}
    D8TextureParameters *p=&r->texture_parameters[texture_slot(texture)];
    if(p->texture!=texture||p->epoch!=r->texture_epoch){p->texture=texture;p->epoch=r->texture_epoch;p->valid=0;}
    GLint value[]={tss[16]>1?GL_LINEAR:GL_NEAREST,tss[17]>1?GL_LINEAR:GL_NEAREST,tss[13]==1?GL_REPEAT:tss[13]==2?GL_MIRRORED_REPEAT:GL_CLAMP_TO_EDGE,tss[14]==1?GL_REPEAT:tss[14]==2?GL_MIRRORED_REPEAT:GL_CLAMP_TO_EDGE};
    static const GLenum name[]={GL_TEXTURE_MAG_FILTER,GL_TEXTURE_MIN_FILTER,GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T};
    for(unsigned i=0;i<4;i++)if(!(p->valid&(1u<<i))||p->parameter[i]!=value[i]){glTexParameteri(GL_TEXTURE_2D,name[i],value[i]);p->parameter[i]=value[i];p->valid|=1u<<i;}
}
/* Cache only values owned by this specific linked GPU program. GL program
 * switches (including the FPS overlay) leave uniform values intact. */
static int gpu_upload_changed(D8GpuUploadCache *c,unsigned bit,void *old,const void *value,size_t size){
    uint32_t mask=1u<<bit;if((c->valid&mask)&&!memcmp(old,value,size))return 0;
    memcpy(old,value,size);c->valid|=mask;return 1;
}
static int gpu_prepare(D8Renderer *r,const D8State *s,unsigned n,const void *data,size_t stride,unsigned vcount,const void *indices,unsigned isize,unsigned base,unsigned stages,const D8NativeBuffers *native){
 unsigned max=0,min=UINT32_MAX,pos=s->fvf&14,tc=(s->fvf>>8)&15;
 if(native&&native->index&&indices&&(native->index_offset>native->index_bytes||(size_t)n*isize>native->index_bytes-native->index_offset))return reject("native index buffer range exceeds allocation: offset=%zu bytes=%zu count=%u size=%u",native->index_offset,native->index_bytes,n,isize);

 for(unsigned i=0;i<n;i++){unsigned ix=i;if(indices){if(isize==2){uint16_t x;memcpy(&x,(const char*)indices+i*2,2);ix=x;}else if(isize==4)memcpy(&ix,(const char*)indices+i*4,4);else return reject("invalid GPU index size %u",isize);}if(ix>=vcount||ix>UINT32_MAX-base)return reject("GPU index outside vertex range: i=%u index=%u count=%u base=%u",i,ix,vcount,base);if(ix>max)max=ix;if(ix<min)min=ix;
 if(pos==4){float rhw;memcpy(&rhw,(const char*)data+(size_t)(ix+base)*stride+12,4);if(!isfinite(rhw))return reject("invalid GPU RHW: vertex=%u rhw=%.9g",i,rhw);}}
 const void *uploaded_indices=indices;unsigned first=r->trim_vertex_prefix&&indices&&!(native&&(native->vertex||native->index))?min:0;
 size_t attribute_base=0;
 if(native&&native->vertex){
     uint64_t index=(uint64_t)base+max;size_t need=d8_fvf_stride(s->fvf);
     if(native->vertex_offset>native->vertex_bytes||index>(native->vertex_bytes-native->vertex_offset)/stride)return reject("native vertex buffer range exceeds allocation: offset=%zu bytes=%zu base=%u max=%u stride=%zu",native->vertex_offset,native->vertex_bytes,base,max,stride);
     size_t offset=native->vertex_offset+(size_t)index*stride;
     if(need>native->vertex_bytes-offset)return reject("native vertex buffer range exceeds allocation: offset=%zu need=%zu bytes=%zu",offset,need,native->vertex_bytes);
     attribute_base=native->vertex_offset+(size_t)base*stride;
 }
 if(first){
     size_t bytes=(size_t)n*isize;if(r->rebased_capacity<bytes){void *scratch=realloc(r->rebased_indices,bytes);if(!scratch)return reject("GPU index rebasing allocation failed: %zu bytes",bytes);r->rebased_indices=scratch;r->rebased_capacity=bytes;}
     for(unsigned i=0;i<n;i++){if(isize==2){uint16_t ix;memcpy(&ix,(const char*)indices+i*2,2);ix=(uint16_t)(ix-first);memcpy((char*)r->rebased_indices+i*2,&ix,2);}else{uint32_t ix;memcpy(&ix,(const char*)indices+i*4,4);ix-=first;memcpy((char*)r->rebased_indices+i*4,&ix,4);}}
     uploaded_indices=r->rebased_indices;
 }
 GLuint p=r->gpu_program;glUseProgram(p);
 const D8UniformLocations *u=&r->gpu_uniforms;D8SmallUniformCache *small=&r->gpu_small_uniforms;
 D8GpuUploadCache *uploaded=&r->gpu_uploaded;
 /* The RHW branch consumes screen coordinates directly. Leave dormant
  * snapshots unchanged so the next XYZ draw uploads its actual state. */
 if(pos!=4){
 float inv[9];
 if(gpu_upload_changed(uploaded,0,uploaded->matrix[0],s->transform[256],64)){
     glUniformMatrix4fv(u->world,1,GL_FALSE,s->transform[256]);
     float nm[9];if(inverse3(s->transform[256],inv)){memset(inv,0,sizeof inv);inv[0]=inv[4]=inv[8]=1;}
     for(int i=0;i<3;i++)for(int j=0;j<3;j++)nm[i*3+j]=inv[j*3+i];glUniformMatrix3fv(u->normalmat,1,GL_FALSE,nm);
 }
 if(gpu_upload_changed(uploaded,1,uploaded->matrix[1],s->transform[2],64)){
     glUniformMatrix4fv(u->view,1,GL_FALSE,s->transform[2]);
     float eye[3]={-s->transform[2][2],-s->transform[2][6],-s->transform[2][10]},camera[3]={0};
     if(!inverse3(s->transform[2],inv))for(int j=0;j<3;j++)camera[j]=-(s->transform[2][12]*inv[j]+s->transform[2][13]*inv[3+j]+s->transform[2][14]*inv[6+j]);
     glUniform3fv(u->eye,1,eye);glUniform3fv(u->camera,1,camera);
 }
 if(gpu_upload_changed(uploaded,2,uploaded->matrix[2],s->transform[3],64))glUniformMatrix4fv(u->projection,1,GL_FALSE,s->transform[3]);
 }
 if(gpu_upload_changed(uploaded,3,uploaded->matrix[3],s->transform[16],64))glUniformMatrix4fv(u->texmat0,1,GL_FALSE,s->transform[16]);
 if(gpu_upload_changed(uploaded,4,uploaded->matrix[4],s->transform[17],64))glUniformMatrix4fv(u->texmat1,1,GL_FALSE,s->transform[17]);
 small4f(small,SU_VIEWPORT,u->viewport,s->viewport[0],s->viewport[1],s->viewport[2],s->viewport[3]);small2f(small,SU_ZRANGE,u->zrange,asfloat(s->viewport[4]),asfloat(s->viewport[5]));
 small4i(small,SU_FLAGS,u->flags,pos==4,s->rs[137]&&(s->fvf&16),s->rs[143],s->rs[35]);
 int lighting=pos!=4&&s->rs[137]&&(s->fvf&16);
 if(lighting){
 unsigned src[4]={s->rs[145],s->rs[147],s->rs[146],s->rs[148]};for(int i=0;i<4;i++)if(!s->rs[141]||(src[i]==1&&!(s->fvf&64))||(src[i]==2&&!(s->fvf&128)))src[i]=0;small4i(small,SU_SOURCES,u->sources,src[0],src[1],src[2],src[3]);
 }
 small4i(small,SU_TEXFLAGS,u->texflags,stages>0?s->tss[0][24]:0,stages>1?s->tss[1][24]:0,stages>0?s->texture_flipy[0]:0,stages>1?s->texture_flipy[1]:0);
 /* Unlit XYZ and RHW draws cannot consume lighting uniforms. Crucially,
  * do not mark deferred values as uploaded when updating guest state. */
 if(lighting){
 if(gpu_upload_changed(uploaded,5,uploaded->material,s->material,sizeof s->material)){glUniform4fv(u->material,4,s->material);glUniform1f(u->power,s->material[16]);}
 if(gpu_upload_changed(uploaded,6,&uploaded->ambient,&s->rs[139],4)){float amb[4];argb(s->rs[139],amb);glUniform4fv(u->ambient,1,amb);}
 small1i(small,SU_LOCALVIEWER,u->localviewer,s->rs[142]!=0);
 int changed=gpu_upload_changed(uploaded,7,uploaded->light,s->light,sizeof s->light);
 changed|=gpu_upload_changed(uploaded,8,&uploaded->specular,&s->rs[29],4);
 if(changed){float lights[56*4]={0};for(int i=0;i<8;i++){float *l=lights+i*28;memcpy(l,s->light[i],26*4);uint32_t type;memcpy(&type,s->light[i],4);l[0]=(float)type;l[26]=s->rs[29]!=0;}glUniform4fv(u->lights,56,lights);}
 if(gpu_upload_changed(uploaded,9,uploaded->light_on,s->light_on,sizeof s->light_on)){float on[8];for(int i=0;i<8;i++)on[i]=s->light_on[i]!=0;glUniform4fv(u->enabled,2,on);}
 }
 size_t vertex_bytes=((size_t)max-first+1)*stride;
 if(native&&native->vertex){glBindBuffer(GL_ARRAY_BUFFER,native->vertex);r->uploads.native_vertex_draws++;}
 else{glBindBuffer(GL_ARRAY_BUFFER,r->gpu_vbo);glBufferData(GL_ARRAY_BUFFER,vertex_bytes,(const char*)data+((size_t)base+first)*stride,GL_STREAM_DRAW);d8_renderer_note_buffer_upload(r,0,vertex_bytes);}
 r->uploads.prefix_vertex_bytes+=((size_t)max+1)*stride;r->uploads.avoidable_prefix_bytes+=(size_t)min*stride;if(first)r->uploads.trimmed_draws++;
 attributes_begin(r);GLuint attribute_buffer=native&&native->vertex?native->vertex:r->gpu_vbo;
 size_t off=pos==4?16:12;gpu_attribute(r,0,pos==4?4:3,GL_FLOAT,0,stride,attribute_base,attribute_buffer);
 if(s->fvf&16){gpu_attribute(r,1,3,GL_FLOAT,0,stride,attribute_base+off,attribute_buffer);off+=12;}else gpu_constant(r,1,0,0,1,0);if(s->fvf&32)off+=4;
 if(s->fvf&64){gpu_attribute(r,2,4,GL_UNSIGNED_BYTE,1,stride,attribute_base+off,attribute_buffer);off+=4;}else gpu_constant(r,2,1,1,1,1);
 if(s->fvf&128){gpu_attribute(r,3,4,GL_UNSIGNED_BYTE,1,stride,attribute_base+off,attribute_buffer);off+=4;}else gpu_constant(r,3,0,0,0,0);
 size_t offsets[8];unsigned dims[8];for(unsigned i=0;i<tc;i++){unsigned d=(s->fvf>>(16+2*i))&3;dims[i]=d==0?2:d==1?3:d==2?4:1;offsets[i]=off;off+=dims[i]*4;}
 for(int t=0;t<2;t++){if((unsigned)t>=stages){gpu_constant(r,4+t,0,0,0,0);continue;}unsigned ix=s->tss[t][11]&65535;if(ix>=8)return reject("GPU invalid texcoord index stage %d index %u",t,ix);if(ix<tc)gpu_attribute(r,4+t,dims[ix],GL_FLOAT,0,stride,attribute_base+offsets[ix],attribute_buffer);else gpu_constant(r,4+t,0,0,0,0);}
 if(indices){if(native&&native->index){glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,native->index);r->uploads.native_index_draws++;}
 else{glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,r->gpu_ibo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,(size_t)n*isize,uploaded_indices,GL_STREAM_DRAW);d8_renderer_note_buffer_upload(r,1,(size_t)n*isize);}}return 0;
}

int d8_renderer_draw_native(D8Renderer *r,const D8State *s,unsigned th,unsigned primitive,unsigned count,const void *data,size_t stride,unsigned vcount,const void *indices,unsigned isize,unsigned base,const GLuint textures[2],const D8NativeBuffers *native){
    draw_error[0]=0;unsigned n=d8_primitive_vertices(primitive,count),need=d8_fvf_stride(s->fvf);if(!r->program||!n||n>300000||!need||stride<need||!data)return reject("invalid draw input: program=%u vertices=%u need_stride=%u stride=%zu fvf=%#x data=%p",r->program,n,need,stride,s->fvf,data);
    if(s->rs[8]!=3||s->rs[151]||s->rs[167]||s->rs[152]){return reject("unsupported RS: FILLMODE=%u VERTEXBLEND=%u INDEXEDVERTEXBLEND=%u CLIPPLANEENABLE=%#x",s->rs[8],s->rs[151],s->rs[167],s->rs[152]);}
    /* COLOROP=DISABLE terminates the cascade. Higher stages and their
     * coordinate generators/operations are dormant, regardless of stale state. */
    unsigned stages=0;
    for(unsigned t=0;t<2&&s->tss[t][1]!=1;t++,stages++){
        /* D3D also terminates when COLORARG1 selects an absent texture.
         * Preserve the incoming diffuse/current alpha instead of substituting
         * an opaque white texture (e.g. the scope's 25%-alpha orange tint). */
        if(!textures[t]&&(s->tss[t][2]&15)==2)break;
        if((s->tss[t][11]&0xffff0000u)||(s->tss[t][1]>16&&s->tss[t][1]!=24)||(s->tss[t][4]>16&&s->tss[t][4]!=24))return reject("unsupported TSS stage=%u COLOROP=%u ALPHAOP=%u TEXCOORDINDEX=%#x",t,s->tss[t][1],s->tss[t][4],s->tss[t][11]);
        unsigned ix=s->tss[t][11]&65535,flags=s->tss[t][24],pc=flags&15;
        if(ix>=8)return reject("texture coordinate index invalid: stage=%u index=%u",t,ix);
        if((flags&256)&&(pc<1||pc>4))return reject("invalid projected texture coordinate count: stage=%u flags=%#x",t,flags);
    }
    int gpu=r->gpu_program!=0;
    /* GLES2 lacks core 32-bit element indices. CPU deindexing remains valid. */
    if(gpu&&indices&&isize==4){const char *version=(const char*)glGetString(GL_VERSION),*ext=(const char*)glGetString(GL_EXTENSIONS);if(!strstr(version,"OpenGL ES 3")&&(!ext||!strstr(ext,"GL_OES_element_index_uint"))){static int warned;if(!warned++){fprintf(stderr,"[d3d8] GPU uint32 indices unsupported; using CPU reference\n");}gpu=0;}}
if(gpu&&gpu_prepare(r,s,n,data,stride,vcount,indices,isize,base,stages,native))return -1;
    GLuint program=gpu?r->gpu_program:r->program;const D8UniformLocations *u=gpu?&r->gpu_uniforms:&r->uniforms;D8SmallUniformCache *small=gpu?&r->gpu_small_uniforms:&r->small_uniforms;
    size_t bytes=(size_t)n*sizeof(Vertex);if(!gpu&&r->capacity<bytes){void *p=realloc(r->vertices,bytes);if(!p)return reject("vertex staging allocation failed: bytes=%zu",bytes);r->vertices=p;r->capacity=bytes;}Vertex *dst=r->vertices;
    unsigned pos=s->fvf&14,tc=(s->fvf>>8)&15;
    for(unsigned i=0;!gpu&&i<n;i++){
        unsigned index=i;if(indices){if(isize==2){uint16_t x;memcpy(&x,(const char*)indices+i*2,2);index=x;}else if(isize==4)memcpy(&index,(const char*)indices+i*4,4);else return reject("invalid index size: %u",isize);}
        if(index>=vcount||index>UINT32_MAX-base)return reject("index outside vertex range: i=%u index=%u count=%u base=%u",i,index,vcount,base);index+=base;const uint8_t *src=(const uint8_t*)data+(size_t)index*stride;
        Vertex *v=dst+i;float p[4]={0,0,0,1},normal[3]={0,0,1};memcpy(p,src,pos==4?16:12);src+=pos==4?16:12;
        if(s->fvf&16){memcpy(normal,src,12);src+=12;}if(s->fvf&32)src+=4;
        uint32_t c=UINT32_MAX,spec=0;if(s->fvf&64){memcpy(&c,src,4);src+=4;}if(s->fvf&128){memcpy(&spec,src,4);src+=4;}argb(c,v->c);argb(spec,v->spec);
        float coords[8][4]={{0}};for(unsigned t=0;t<tc;t++){unsigned dim=(s->fvf>>(16+2*t))&3;dim=dim==0?2:dim==1?3:dim==2?4:1;coords[t][3]=1;memcpy(coords[t],src,dim*4);src+=dim*4;}
        for(int t=0;t<2;t++){if((unsigned)t>=stages){v->uv[t][0]=v->uv[t][1]=0;continue;}unsigned ix=s->tss[t][11]&0xffff;if(ix>=8)return reject("texture coordinate index invalid: stage=%d index=%u",t,ix);float uv[4];memcpy(uv,coords[ix],16);if(s->tss[t][24])vector(uv,uv,s->transform[16+t]);unsigned projected_count=s->tss[t][24]&15;if((s->tss[t][24]&256)&&(projected_count<1||projected_count>4))return reject("invalid projected texture coordinate count: stage=%d flags=%#x",t,s->tss[t][24]);float q=(s->tss[t][24]&256)?uv[projected_count-1]:1;if(q==0)q=1;v->uv[t][0]=uv[0]/q;v->uv[t][1]=uv[1]/q;if(s->texture_flipy[t])v->uv[t][1]=1-v->uv[t][1];}
        if(pos==4){if(!isfinite(p[3]))return reject("invalid RHW: vertex=%u position=(%.9g,%.9g,%.9g) rhw=%.9g",i,p[0],p[1],p[2],p[3]);if(p[3]==0)p[3]=1;float x=(p[0]+.5f-s->viewport[0])/s->viewport[2],y=(p[1]+.5f-s->viewport[1])/s->viewport[3];v->p[0]=(2*x-1)/p[3];v->p[1]=(1-2*y)/p[3];float zmin=asfloat(s->viewport[4]),zmax=asfloat(s->viewport[5]);float z=zmax!=zmin?(p[2]-zmin)/(zmax-zmin):0;v->p[2]=(2*z-1)/p[3];v->p[3]=1/p[3];v->fog=s->rs[35]?p[2]:v->spec[3];}
        else{float world[4],view[4];vector(world,p,s->transform[256]);vector(view,world,s->transform[2]);vector(v->p,view,s->transform[3]);v->p[2]=2*v->p[2]-v->p[3];v->fog=fabsf(view[2]);if(s->rs[137]&&(s->fvf&16))light_vertex(s,world,normal,v->c,v->spec);}
    }
    /* gpu_prepare already made this program current before its uniforms. */
    if(!gpu)glUseProgram(program);raster(r,s,th);
    for(int t=0;t<2;t++){GLuint texture=(unsigned)t<stages?textures[t]:0;if(texture||!native||!native->default_framebuffer){glActiveTexture(GL_TEXTURE0+t);glBindTexture(GL_TEXTURE_2D,texture);if(texture)texture_parameters(r,texture,s->tss[t]);}
        small1i(small,SU_HAS0+t,u->has[t],texture!=0);small4i(small,SU_COP0+t,u->cop[t],(unsigned)t<stages?s->tss[t][1]:1,s->tss[t][2],s->tss[t][3],0);small4i(small,SU_AOP0+t,u->aop[t],s->tss[t][4],s->tss[t][5],s->tss[t][6],0);}
    float fc[4],factor[4];argb(s->rs[34],fc);argb(s->rs[60],factor);small4fv(small,SU_FACTOR,u->factor,factor);small4fv(small,SU_FOGCOLOR,u->fogcolor,fc);
    small1i(small,SU_AFUNC,u->afunc,s->rs[15]?s->rs[25]:8);small1f(small,SU_AREF,u->aref,(s->rs[24]&255)/255.f);
    unsigned fog=s->rs[35]?s->rs[35]:pos==4?4:s->rs[140];small1i(small,SU_FOGMODE,u->fogmode,s->rs[28]?(GLint)fog:0);small3f(small,SU_FOGPARAM,u->fogparam,asfloat(s->rs[36]),asfloat(s->rs[37]),asfloat(s->rs[38]));small1i(small,SU_SPECULAR,u->specular,s->rs[29]!=0);
    if(!gpu){glBindBuffer(GL_ARRAY_BUFFER,r->vbo);glBufferData(GL_ARRAY_BUFFER,bytes,dst,GL_STREAM_DRAW);r->uploads.vertex_bytes+=bytes;r->uploads.prefix_vertex_bytes+=bytes;r->uploads.vertex_uploads++;size_t offsets[]={offsetof(Vertex,p),offsetof(Vertex,c),offsetof(Vertex,spec),offsetof(Vertex,uv[0]),offsetof(Vertex,uv[1]),offsetof(Vertex,fog)};int sizes[]={4,4,4,2,2,1};attributes_begin(r);for(int i=0;i<6;i++)gpu_attribute(r,i,sizes[i],GL_FLOAT,GL_FALSE,sizeof(Vertex),offsets[i],r->vbo);
    }
    static const GLenum modes[]={0,GL_POINTS,GL_LINES,GL_LINE_STRIP,GL_TRIANGLES,GL_TRIANGLE_STRIP,GL_TRIANGLE_FAN};if(gpu&&indices)glDrawElements(modes[primitive],n,isize==2?GL_UNSIGNED_SHORT:GL_UNSIGNED_INT,(void*)(native&&native->index?native->index_offset:0));else glDrawArrays(modes[primitive],0,n);GLenum e=glGetError();if(e){d8_renderer_invalidate_raster(r);d8_renderer_invalidate_attributes(r);d8_renderer_invalidate_texture_parameters(r);r->small_uniforms.valid=r->gpu_small_uniforms.valid=0;r->gpu_uploaded.valid=0;return reject("GL draw error %#x",e);}return 0;
}

int d8_renderer_draw(D8Renderer *r,const D8State *s,unsigned th,unsigned primitive,unsigned count,const void *data,size_t stride,unsigned vcount,const void *indices,unsigned isize,unsigned base,const GLuint textures[2]){
    return d8_renderer_draw_native(r,s,th,primitive,count,data,stride,vcount,indices,isize,base,textures,NULL);
}
