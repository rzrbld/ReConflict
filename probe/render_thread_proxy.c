/* Experimental opt-in native GL command proxy. Build separately, never in the
 * normal runtime wildcard. One producer/thread/context; no guest state access. */
#include "render_thread_proxy.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#define CAPACITY (8u*1024u*1024u)
#include "render_proxy_ops.inc"
typedef void (*Execute)(void *);
typedef struct {uint32_t bytes,op;uint64_t sequence;uint32_t guest,frame;Execute execute;} Header;
_Static_assert(sizeof(Header)==32,"queue header alignment");
static struct {
 unsigned char *arena;uint64_t head,next_sequence;_Atomic uint64_t published,consumed,completed;
 _Atomic int active,stopping,failed;SDL_mutex *mutex;SDL_cond *work,*space;SDL_sem *ready;SDL_Thread *thread;
 SDL_Window *window;SDL_GLContext context;SDL_threadID producer;uint32_t guest,frame;DsRtError error;
} q;
static _Thread_local int worker_thread;
static GLint proxy_unpack_alignment=4,proxy_unpack_row_length,proxy_unpack_skip_rows,proxy_unpack_skip_pixels;
static GLuint proxy_unpack_buffer;
static GLuint proxy_array_buffer,proxy_element_buffer,proxy_attrib_buffer[64];
static uint64_t proxy_enabled_mask,proxy_client_mask;
static int proxy_client_unknown,proxy_client_ever,native_attrib_count;
static unsigned producer_last_draw_op;
static int native_es3;
extern GLenum __real_glGetError(void);
extern void __real_glGetIntegerv(GLenum,GLint *);
extern const GLubyte *__real_glGetString(GLenum);
extern void __real_SDL_GL_SwapWindow(SDL_Window *);
extern int __real_SDL_GL_SetSwapInterval(int);
static int proxy_active(void){return atomic_load_explicit(&q.active,memory_order_acquire);}
int ds_rt_active(void){return proxy_active();}
size_t ds_rt_capacity(void){return CAPACITY;}
const DsRtError *ds_rt_error(void){
 int state;while((state=atomic_load_explicit(&q.failed,memory_order_acquire))==2)SDL_Delay(0);
 return state==1?&q.error:NULL;
}
static void fail(uint32_t code,const Header *h,const char *api,const char *message){
 int empty=0;
 if(!atomic_compare_exchange_strong_explicit(&q.failed,&empty,2,memory_order_acq_rel,memory_order_acquire))return;
 memset(&q.error,0,sizeof q.error);q.error.code=code;q.error.guest=h?h->guest:0;q.error.frame=h?h->frame:0;q.error.sequence=h?h->sequence:0;
 snprintf(q.error.api,sizeof q.error.api,"%s",api);snprintf(q.error.message,sizeof q.error.message,"%s",message?message:"");
 atomic_store_explicit(&q.failed,1,memory_order_release);
}
static void publish(void){
 if(atomic_load_explicit(&q.published,memory_order_relaxed)==q.head)return;
 atomic_store_explicit(&q.published,q.head,memory_order_release);
 SDL_LockMutex(q.mutex);SDL_CondSignal(q.work);SDL_UnlockMutex(q.mutex);
}
static void wait_space(size_t bytes){
 if(q.head-atomic_load_explicit(&q.consumed,memory_order_acquire)+bytes>CAPACITY){
  publish();SDL_LockMutex(q.mutex);
  while(q.head-atomic_load_explicit(&q.consumed,memory_order_acquire)+bytes>CAPACITY)SDL_CondWait(q.space,q.mutex);
  SDL_UnlockMutex(q.mutex);
 }
}
static Header *reserve(size_t bytes){
 if(SDL_ThreadID()!=q.producer){fprintf(stderr,"[render-thread] multiple producers are unsupported\n");abort();}
 if(bytes>CAPACITY||bytes>SIZE_MAX-31){fprintf(stderr,"[render-thread] invalid internal record size\n");abort();}
 bytes=(bytes+31)&~(size_t)31;
 size_t offset=q.head&(CAPACITY-1),pad=offset+bytes>CAPACITY?CAPACITY-offset:0;
 if(pad){wait_space(pad);Header *p=(Header*)(q.arena+offset);memset(p,0,sizeof *p);p->bytes=(uint32_t)pad;q.head+=pad;offset=0;}
 wait_space(bytes);
 Header *h=(Header*)(q.arena+offset);memset(h,0,sizeof *h);h->bytes=(uint32_t)bytes;h->sequence=++q.next_sequence;h->guest=q.guest;h->frame=q.frame;q.head+=bytes;return h;
}
static int issue(Execute execute,unsigned op,const void *args,size_t size,const void *data,size_t bytes,size_t pointer_offset,int synchronous){
 if(size>CAPACITY-sizeof(Header)-32){fprintf(stderr,"[render-thread] invalid command argument size\n");abort();}
 if(bytes>CAPACITY-sizeof(Header)-size-32){synchronous=1;bytes=0;}
 if(synchronous)bytes=0;
 Header *h=reserve(sizeof *h+size+(data?bytes:0));h->op=op;h->execute=execute;void *body=h+1;memcpy(body,args,size);
 if(data&&bytes){void *copy=(unsigned char*)body+size;memcpy(copy,data,bytes);memcpy((unsigned char*)body+pointer_offset,&copy,sizeof copy);}
 uint64_t sequence=h->sequence;
 if(synchronous){
  publish();SDL_LockMutex(q.mutex);while(atomic_load_explicit(&q.completed,memory_order_acquire)<sequence)SDL_CondWait(q.space,q.mutex);SDL_UnlockMutex(q.mutex);
 }
 return ds_rt_error()?-1:0;
}
static int texture_payload(GLsizei width,GLsizei height,GLenum format,GLenum type,size_t *bytes){
 if(proxy_unpack_buffer)return 1; /* pointer is an offset, preserve by RPC */
 if(proxy_unpack_row_length||proxy_unpack_skip_rows||proxy_unpack_skip_pixels||width<0||height<0||width>65536||height>65536)return 1;
 unsigned components=format==GL_RGBA?4:format==GL_RGB?3:format==GL_LUMINANCE_ALPHA?2:format==GL_ALPHA||format==GL_LUMINANCE?1:0;
 unsigned unit=type==GL_UNSIGNED_BYTE?components:type==GL_UNSIGNED_SHORT_5_6_5||type==GL_UNSIGNED_SHORT_4_4_4_4||type==GL_UNSIGNED_SHORT_5_5_5_1?2:0;
 if(!unit||proxy_unpack_alignment<1||proxy_unpack_alignment>8||(proxy_unpack_alignment&(proxy_unpack_alignment-1)))return 1;
 size_t row=(size_t)width*unit,stride=(row+proxy_unpack_alignment-1)&~(size_t)(proxy_unpack_alignment-1);
 *bytes=height?(height-1)*stride+row:0;return 0;
}
static void barrier(void *unused){(void)unused;}
static void error_check(void *unused){(void)unused;}
typedef struct {GLint alignment,row_length,skip_rows,skip_pixels,buffer,array,element,attrib_buffer[64];uint64_t enabled;int client_seen;} UnpackState;
typedef struct {void (*fn)(void *);void *arg;UnpackState *reply;} NativeJob;
static void unpack_snapshot(UnpackState *s){
 memset(s,0,sizeof *s);__real_glGetIntegerv(GL_UNPACK_ALIGNMENT,&s->alignment);
 if(native_es3){__real_glGetIntegerv(0x0cf2,&s->row_length);__real_glGetIntegerv(0x0cf3,&s->skip_rows);__real_glGetIntegerv(0x0cf4,&s->skip_pixels);__real_glGetIntegerv(0x88ef,&s->buffer);}
 __real_glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&s->array);__real_glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&s->element);
 extern void __real_glGetVertexAttribiv(GLuint,GLenum,GLint *);
 extern void __real_glGetVertexAttribPointerv(GLuint,GLenum,void **);
 for(int i=0;i<native_attrib_count&&i<64;i++){
  GLint enabled;__real_glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&enabled);__real_glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&s->attrib_buffer[i]);if(enabled)s->enabled|=UINT64_C(1)<<i;
  if(!s->attrib_buffer[i]){void *pointer=NULL;__real_glGetVertexAttribPointerv(i,GL_VERTEX_ATTRIB_ARRAY_POINTER,&pointer);if(enabled||pointer)s->client_seen=1;}
 }
}
static void unpack_restore_shadow(const UnpackState *s){
 proxy_unpack_alignment=s->alignment;proxy_unpack_row_length=s->row_length;proxy_unpack_skip_rows=s->skip_rows;proxy_unpack_skip_pixels=s->skip_pixels;proxy_unpack_buffer=(GLuint)s->buffer;
 proxy_array_buffer=(GLuint)s->array;proxy_element_buffer=(GLuint)s->element;proxy_enabled_mask=s->enabled;proxy_client_mask=0;proxy_client_unknown=native_attrib_count>64;
 if(s->client_seen)proxy_client_ever=1;
 for(unsigned i=0;i<64;i++){proxy_attrib_buffer[i]=(GLuint)s->attrib_buffer[i];if(!proxy_attrib_buffer[i])proxy_client_mask|=proxy_enabled_mask&(UINT64_C(1)<<i);}
}
static int client_arrays(void){return proxy_client_ever||proxy_client_unknown||proxy_client_mask!=0;}
static void attribute_shadow(GLuint index,int enabled,int pointer){
 if(index>=64){proxy_client_unknown=1;return;}uint64_t bit=UINT64_C(1)<<index;
 if(pointer)proxy_attrib_buffer[index]=proxy_array_buffer;
 else if(enabled)proxy_enabled_mask|=bit;else proxy_enabled_mask&=~bit;
 if((proxy_enabled_mask&bit)&&!proxy_attrib_buffer[index])proxy_client_mask|=bit;else proxy_client_mask&=~bit;
}
static void attribute_pointer_shadow(GLuint index,GLint size,GLenum type,GLsizei stride,const void *pointer){
 if(!proxy_array_buffer)proxy_client_ever=1;
 unsigned bytes=type==GL_BYTE||type==GL_UNSIGNED_BYTE?1:type==GL_SHORT||type==GL_UNSIGNED_SHORT?2:type==GL_FLOAT||type==GL_FIXED?4:0;
 /* A rejected GL call leaves the previous descriptor intact. Never turn an
  * actual client array into an apparently safe VBO from speculative state. */
 if(index>=(GLuint)native_attrib_count||size<1||size>4||stride<0||stride>2048||!bytes||((uintptr_t)pointer&(bytes-1))){proxy_client_unknown=1;return;}
 attribute_shadow(index,0,1);
}
static void deleted_buffer(GLuint buffer){
 if(proxy_unpack_buffer==buffer)proxy_unpack_buffer=0;
 if(proxy_array_buffer==buffer)proxy_array_buffer=0;
 if(proxy_element_buffer==buffer)proxy_element_buffer=0;
 for(unsigned i=0;i<64;i++)if(proxy_attrib_buffer[i]==buffer){proxy_attrib_buffer[i]=0;proxy_client_mask|=proxy_enabled_mask&(UINT64_C(1)<<i);}
}
static void native_job(void *p){NativeJob *j=p;j->fn(j->arg);if(j->reply)unpack_snapshot(j->reply);}
static int worker(void *unused){
 (void)unused;worker_thread=1;
 if(SDL_GL_MakeCurrent(q.window,q.context)){fail(UINT32_MAX,NULL,"SDL_GL_MakeCurrent",SDL_GetError());SDL_SemPost(q.ready);return -1;}
 SDL_SemPost(q.ready);uint64_t tail=0;
 for(;;){
  SDL_LockMutex(q.mutex);
  while(tail==atomic_load_explicit(&q.published,memory_order_acquire)&&!atomic_load(&q.stopping))SDL_CondWait(q.work,q.mutex);
  SDL_UnlockMutex(q.mutex);
  uint64_t end=atomic_load_explicit(&q.published,memory_order_acquire);
  if(tail==end&&atomic_load(&q.stopping))break;
  while(tail<end){
   Header *h=(Header*)(q.arena+(tail&(CAPACITY-1)));
   if(h->op==OP_DRAW_CHECK){GLenum e=__real_glGetError();unsigned op;memcpy(&op,h+1,sizeof op);if(e)fail(e,h,op<OP_COUNT?op_names[op]:"draw-error-check","driver error observed at explicit deferred post-draw check");}
   else if(h->execute)h->execute(h+1);
   tail+=h->bytes;
   if(h->sequence)atomic_store_explicit(&q.completed,h->sequence,memory_order_release);
   atomic_store_explicit(&q.consumed,tail,memory_order_release);
  }
  SDL_LockMutex(q.mutex);SDL_CondBroadcast(q.space);SDL_UnlockMutex(q.mutex);
 }
 if(SDL_GL_MakeCurrent(NULL,NULL))fail(UINT32_MAX,NULL,"SDL_GL_MakeCurrent","worker release failed");
 return ds_rt_error()?-1:0;
}
void ds_rt_origin(uint32_t guest,uint32_t frame){q.guest=guest;q.frame=frame;}
int ds_rt_sync(void){if(!proxy_active()||worker_thread)return ds_rt_error()?-1:0;int zero=0;return issue(barrier,OP_BARRIER,&zero,sizeof zero,NULL,0,SIZE_MAX,1);}
int ds_rt_call(void (*fn)(void *),void *arg){
 if(!fn)return -1;
 if(!proxy_active()||worker_thread){fn(arg);return ds_rt_error()?-1:0;}
 UnpackState unpack;NativeJob job={fn,arg,&unpack};int result=issue(native_job,OP_NATIVE_JOB,&job,sizeof job,NULL,0,SIZE_MAX,1);unpack_restore_shadow(&unpack);return result;
}
int ds_rt_submit(void (*fn)(void *),const void *payload,size_t bytes){
 if(!fn||(!payload&&bytes))return -1;
 if(!proxy_active()||worker_thread){fn((void*)payload);return ds_rt_error()?-1:0;}
 NativeJob job={fn,(void*)payload,NULL};int result=issue(native_job,OP_NATIVE_JOB,&job,sizeof job,payload,bytes,offsetof(NativeJob,arg),0);publish();return result;
}
GLenum ds_rt_defer_draw_error(void){
 if(!proxy_active()||worker_thread)return __real_glGetError();
 unsigned op=producer_last_draw_op?producer_last_draw_op:OP_DRAW_CHECK;
 issue(error_check,OP_DRAW_CHECK,&op,sizeof op,NULL,0,SIZE_MAX,0);publish();return GL_NO_ERROR;
}
static void destroy_queue(void){
 if(q.ready)SDL_DestroySemaphore(q.ready);
 if(q.work)SDL_DestroyCond(q.work);
 if(q.space)SDL_DestroyCond(q.space);
 if(q.mutex)SDL_DestroyMutex(q.mutex);
 free(q.arena);
 q.ready=NULL;q.work=q.space=NULL;q.mutex=NULL;q.arena=NULL;q.thread=NULL;
}
int ds_rt_start(SDL_Window *window,SDL_GLContext context){
 if(proxy_active())return -1;
 const char *enabled=getenv("DS_RENDER_THREAD");if(!enabled||strcmp(enabled,"1"))return 0;
 if(q.context!=context)proxy_client_ever=0;
 memset(&q,0,sizeof q);q.window=window;q.context=context;q.producer=SDL_ThreadID();
 q.arena=malloc(CAPACITY);q.mutex=SDL_CreateMutex();q.work=SDL_CreateCond();q.space=SDL_CreateCond();q.ready=SDL_CreateSemaphore(0);
 if(!q.arena||!q.mutex||!q.work||!q.space||!q.ready){fail(UINT32_MAX,NULL,"queue-init","allocation failed");destroy_queue();return -1;}
 proxy_unpack_alignment=4;proxy_unpack_row_length=proxy_unpack_skip_rows=proxy_unpack_skip_pixels=0;proxy_unpack_buffer=0;
 producer_last_draw_op=0;
 const char *version=(const char*)__real_glGetString(GL_VERSION);native_es3=version&&strstr(version,"OpenGL ES 3");__real_glGetIntegerv(GL_MAX_VERTEX_ATTRIBS,&native_attrib_count);UnpackState unpack;unpack_snapshot(&unpack);unpack_restore_shadow(&unpack);
 if(SDL_GL_MakeCurrent(NULL,NULL)){fail(UINT32_MAX,NULL,"SDL_GL_MakeCurrent",SDL_GetError());destroy_queue();return -1;}
 q.thread=SDL_CreateThread(worker,"DS-GL-worker",NULL);
 if(!q.thread){fail(UINT32_MAX,NULL,"SDL_CreateThread",SDL_GetError());SDL_GL_MakeCurrent(window,context);destroy_queue();return -1;}
 SDL_SemWait(q.ready);
 if(ds_rt_error()){SDL_WaitThread(q.thread,NULL);SDL_GL_MakeCurrent(window,context);destroy_queue();return -1;}
 atomic_store_explicit(&q.active,1,memory_order_release);return 0;
}
int ds_rt_stop(void){
 if(!proxy_active())return ds_rt_error()?-1:0;
 if(worker_thread)return -1;
 ds_rt_sync();atomic_store(&q.stopping,1);SDL_LockMutex(q.mutex);SDL_CondSignal(q.work);SDL_UnlockMutex(q.mutex);SDL_WaitThread(q.thread,NULL);
 atomic_store_explicit(&q.active,0,memory_order_release);
 if(SDL_GL_MakeCurrent(q.window,q.context))fail(UINT32_MAX,NULL,"SDL_GL_MakeCurrent","main reacquire failed");
 destroy_queue();return ds_rt_error()?-1:0;
}
typedef struct {SDL_Window *window;} Swap;
static void swap_job(void *p){Swap *s=p;SDL_ClearError();__real_SDL_GL_SwapWindow(s->window);if(*SDL_GetError())fail(UINT32_MAX,NULL,"SDL_GL_SwapWindow",SDL_GetError());}
void __wrap_SDL_GL_SwapWindow(SDL_Window *window){if(!proxy_active()||worker_thread){__real_SDL_GL_SwapWindow(window);return;}Swap s={window};ds_rt_call(swap_job,&s);}
typedef struct {int interval,result;char error[192];} Interval;
static void interval_job(void *p){Interval *i=p;i->result=__real_SDL_GL_SetSwapInterval(i->interval);if(i->result<0)snprintf(i->error,sizeof i->error,"%s",SDL_GetError());}
int __wrap_SDL_GL_SetSwapInterval(int interval){if(!proxy_active()||worker_thread)return __real_SDL_GL_SetSwapInterval(interval);Interval i={interval,0,{0}};ds_rt_call(interval_job,&i);if(i.result<0)SDL_SetError("%s",i.error);return i.result;}
#include "render_proxy_wrappers.inc"
