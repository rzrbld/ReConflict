#include <GLES2/gl2.h>
#include <assert.h>
void test_gl_cost_calls(void){
    glDrawArrays(GL_TRIANGLES,7,3);glBindBuffer(GL_ARRAY_BUFFER,123);
    assert(glGetError()==GL_INVALID_OPERATION);
}
