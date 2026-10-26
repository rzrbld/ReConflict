#include <GLES2/gl2.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern void ds_gl_cost_present(void);
extern void ds_gl_cost_flush(void);
extern int ds_gl_cost_begin(unsigned);
static unsigned draws,binds,errors;
/* --wrap references forward to these exact stubs, not a graphics driver. */
void glDrawArrays(GLenum mode,GLint first,GLsizei count){assert(mode==GL_TRIANGLES&&first==7&&count==3);draws++;}
void glBindBuffer(GLenum target,GLuint buffer){assert(target==GL_ARRAY_BUFFER&&buffer==123);binds++;}
GLenum glGetError(void){errors++;return GL_INVALID_OPERATION;}
/* Separate caller translation unit prevents local calls bypassing --wrap. */
extern void test_gl_cost_calls(void);
int main(void){
    test_gl_cost_calls(); /* forwarding remains valid while disarmed */
    ds_gl_cost_present(); /* env arms next complete frame */
    assert(ds_gl_cost_begin(2)==-1);
    test_gl_cost_calls();ds_gl_cost_present();
    test_gl_cost_calls();test_gl_cost_calls();ds_gl_cost_present();
    test_gl_cost_calls();ds_gl_cost_present(); /* fixed window finished */
    assert(draws==5&&binds==5&&errors==5);
    char first[4096],saved[4096];
    assert(snprintf(first,sizeof first,"%s/gl-cost.csv",getenv("DS_ROOT"))<(int)sizeof first);
    assert(snprintf(saved,sizeof saved,"%s/first.csv",getenv("DS_ROOT"))<(int)sizeof saved);
    assert(!rename(first,saved));
    assert(ds_gl_cost_begin(0)==-1&&ds_gl_cost_begin(601)==-1);
    /* Parent can live-arm another window after the first one finished. */
    assert(ds_gl_cost_begin(2)==0);
    test_gl_cost_calls();ds_gl_cost_present();
    ds_gl_cost_flush(); /* one completed frame, no partial frame */
    assert(draws==6&&binds==6&&errors==6);
    puts("GL cost forwarding/return/window/rearm/flush PASS");return 0;
}
