/* Includes the actual facade implementation; unused methods are link-GC'd. */
#define _POSIX_C_SOURCE 200809L
#ifndef DS_FACADE_SOURCE
#define DS_FACADE_SOURCE "../runtime/d3d8.c"
#endif
#include DS_FACADE_SOURCE
#include <assert.h>
ptrdiff_t g_mem_base;
uint32_t g_cur_func;
static GLuint current_fbo;
static GLenum status=GL_FRAMEBUFFER_COMPLETE;
static unsigned queries,binds;
void glGetIntegerv(GLenum name,GLint *out){assert(name==GL_FRAMEBUFFER_BINDING);queries++;*out=(GLint)current_fbo;}
void glBindFramebuffer(GLenum target,GLuint fbo){assert(target==GL_FRAMEBUFFER);binds++;current_fbo=fbo;}
GLenum glCheckFramebufferStatus(GLenum target){assert(target==GL_FRAMEBUFFER);return status;}
void glFramebufferTexture2D(GLenum a,GLenum b,GLenum c,GLuint d,GLint e){(void)a;(void)b;(void)c;(void)d;(void)e;}
void glFramebufferRenderbuffer(GLenum a,GLenum b,GLenum c,GLuint d){(void)a;(void)b;(void)c;(void)d;}
static uint32_t fixture(unsigned i,uint32_t kind){uint32_t h=0x100+i*32;MEM32(h+8)=i;objects[i].self=h;objects[i].kind=kind;return h;}
int main(void){
 g_mem_base=(ptrdiff_t)calloc(1,4096);assert(g_mem_base);
 uint32_t back=fixture(1,SURFACE),texture=fixture(2,TEXTURE),target=fixture(3,SURFACE),target2=fixture(4,SURFACE);
 objects[1].backbuffer=1;objects[2].texture=22;objects[2].levels=1;
 objects[3].parent=texture;objects[3].fbo=33;objects[4].parent=texture;objects[4].fbo=44;
 render_target=back;depth_target=0;current_fbo=0;status=GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
 assert(bind_target(target,0)==-1);assert(current_fbo==0&&render_target==back&&depth_target==0);assert(queries==1&&binds==2);
 puts("incomplete_offscreen_restores_default_native_and_logical_target=PASS");
 status=GL_FRAMEBUFFER_COMPLETE;assert(!bind_target(target,0));assert(current_fbo==33&&render_target==target);
 puts("successful_offscreen_selects_native_and_logical_target=PASS");
 assert(!bind_target(render_target,depth_target)&&current_fbo==33&&render_target==target);
 puts("resolved_NULL_color_rebinds_current_offscreen=PASS");
 status=GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;assert(bind_target(target2,0)==-1);assert(current_fbo==33&&render_target==target);
 puts("incomplete_offscreen_restores_previous_nondefault_FBO=PASS");
 unsigned before_queries=queries;assert(!bind_target(back,0));assert(current_fbo==0&&render_target==back&&queries==before_queries);
 assert(bind_target(0,0)==-1&&current_fbo==0&&render_target==back&&queries==before_queries);
 puts("default_and_invalid_targets_need_no_framebuffer_query=PASS");
 free((void*)g_mem_base);return 0;
}
