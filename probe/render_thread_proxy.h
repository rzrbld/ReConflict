#ifndef DS_RENDER_THREAD_PROXY_H
#define DS_RENDER_THREAD_PROXY_H
#include <SDL.h>
#include <GLES2/gl2.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
 uint32_t code,guest,frame;uint64_t sequence;
 char api[48],message[192];
} DsRtError;
/* Main/producers only. Start is disabled unless DS_RENDER_THREAD=1. Start
 * after GL initialization; stop before existing GL cleanup. Stop restores
 * the context to main. Only one producer, context and worker are supported. */
int ds_rt_start(SDL_Window *,SDL_GLContext);
int ds_rt_active(void);
void ds_rt_origin(uint32_t guest,uint32_t frame);
int ds_rt_sync(void);
int ds_rt_stop(void);
const DsRtError *ds_rt_error(void);
/* Blocking native-only job, arg remains live until return. No HLE/register
 * access in the job. Worker GL calls bypass the proxy. */
int ds_rt_call(void (*fn)(void *),void *arg);
/* Copied native-only job, published immediately. Primarily for isolated
 * lifetime/backpressure tests. fn must not retain its temporary argument or
 * change buffer/attribute/pixel-unpack state (opaque async jobs cannot update
 * producer shadows). Blocking ds_rt_call resnapshots those states. */
int ds_rt_submit(void (*fn)(void *),const void *payload,size_t bytes);
size_t ds_rt_capacity(void);
/* Replace ONLY the renderer's explicit POST-DRAW error check. Active mode
 * returns provisional NO_ERROR; worker latches the actual draw error and
 * sync/Present returns failure. Plain glGetError stays synchronous. */
GLenum ds_rt_defer_draw_error(void);
#endif
