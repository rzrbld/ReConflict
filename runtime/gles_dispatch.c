/* Resolve GLES through the current SDL context, without a vendor-library link.
 * The existing renderer owns context/thread synchronization. Per-thread caches
 * also keep synchronous and render-worker operation independent. */
#include <SDL.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
static void *resolve_gl(const char *name){
    void *entry=SDL_GL_GetCurrentContext()?SDL_GL_GetProcAddress(name):NULL;
    if(!entry){fprintf(stderr,"[gles-loader] Cannot resolve %s: %s\n",name,SDL_GetError());abort();}
    return entry;
}
/* Do not interpose these trampolines inside SDL or a vendor graphics library. */
#pragma GCC visibility push(hidden)
#define R(ret,name,params,args) ret GL_APIENTRY name params { \
    typedef ret (GL_APIENTRYP Proc) params; \
    static _Thread_local Proc entry; \
    if(!entry)entry=(Proc)resolve_gl(#name); \
    return entry args; }
#define V(name,params,args) R(void,name,params,args)
#include "gl_cost_calls.def"
#include "render_thread_extra.def"
#undef V
#undef R
#pragma GCC visibility pop
