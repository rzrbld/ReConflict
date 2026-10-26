#!/usr/bin/env python3
"""Batch draw publication and suppress redundant condition notifications."""
from pathlib import Path
import hashlib,json,shutil
root=Path(__file__).resolve().parents[1];base=root/'build/render-thread-stage/client-reset-cacheline';data=(base/'render_thread_proxy.c').read_bytes()
assert hashlib.sha256(data).hexdigest()=='7b986db6c6944c6db8ceb89328ef9a228e2a0b9c7f2d713adf21228bf9282514'
s=data.decode()
def replace(old,new,count=1):
 global s
 assert s.count(old)==count,(s.count(old),old[:100])
 s=s.replace(old,new)
replace('uint64_t head,next_sequence,cached_consumed;uint32_t guest,frame;', 'uint64_t head,next_sequence,cached_consumed;uint32_t guest,frame;unsigned draw_publish_count,draw_publish_limit;')
replace('SDL_Window *window;SDL_GLContext context;SDL_threadID producer;DsRtError error;', '''SDL_Window *window;SDL_GLContext context;SDL_threadID producer;DsRtError error;
 /* Both waiter flags are accessed only while q.mutex is held. */
 int worker_waiting,producer_waiting;''')
replace('atomic_store_explicit(&q.published,q.head,memory_order_release);\n SDL_LockMutex(q.mutex);SDL_CondSignal(q.work);SDL_UnlockMutex(q.mutex);', '''atomic_store_explicit(&q.published,q.head,memory_order_release);q.draw_publish_count=0;
 SDL_LockMutex(q.mutex);if(q.worker_waiting)SDL_CondSignal(q.work);SDL_UnlockMutex(q.mutex);''')
replace('SDL_CondWait(q.space,q.mutex);', 'do{q.producer_waiting=1;SDL_CondWait(q.space,q.mutex);q.producer_waiting=0;}while(0);',2)
replace('while(tail==atomic_load_explicit(&q.published,memory_order_acquire)&&!atomic_load(&q.stopping))SDL_CondWait(q.work,q.mutex);', '''while(tail==atomic_load_explicit(&q.published,memory_order_acquire)&&!atomic_load(&q.stopping)){
   q.worker_waiting=1;SDL_CondWait(q.work,q.mutex);q.worker_waiting=0;
  }''')
replace('SDL_LockMutex(q.mutex);SDL_CondBroadcast(q.space);SDL_UnlockMutex(q.mutex);','SDL_LockMutex(q.mutex);if(q.producer_waiting)SDL_CondBroadcast(q.space);SDL_UnlockMutex(q.mutex);')
replace('issue(error_check,OP_DRAW_CHECK,&op,sizeof op,NULL,0,SIZE_MAX,0);publish();return GL_NO_ERROR;', '''issue(error_check,OP_DRAW_CHECK,&op,sizeof op,NULL,0,SIZE_MAX,0);
 /* Each error-check record stays immediately after its draw. Only publication
  * is batched. Queries, capacity pressure, native jobs and Present still flush. */
 if(++q.draw_publish_count>=q.draw_publish_limit)publish();
 return GL_NO_ERROR;''')
replace('producer_last_draw_op=0;', '''producer_last_draw_op=0;
 q.draw_publish_limit=1;
 const char *batch=getenv("DS_RT_DRAW_BATCH");
 if(batch){char *end;unsigned long n=strtoul(batch,&end,10);if(end!=batch&&!*end&&n>=1&&n<=32)q.draw_publish_limit=(unsigned)n;}
 fprintf(stderr,"[render-thread] draw publication batch=%u (waiter-aware notifications)\\n",q.draw_publish_limit);''')
out=root/'build/render-thread-stage/wake-batch';out.mkdir(parents=True,exist_ok=True)
(out/'render_thread_proxy.c').write_text(s)
for name in ('render_thread_proxy.h','render_proxy_ops.inc','render_proxy_wrappers.inc'):shutil.copyfile(base/name,out/name)
(out/'inputs.json').write_text(json.dumps({'base_sha256':hashlib.sha256(data).hexdigest(),'output_sha256':hashlib.sha256(s.encode()).hexdigest(),'scope':'Mutex-protected waiter notifications; optional DS_RT_DRAW_BATCH 1..32, default1. Draw/error ordering, explicit sync, capacity pressure, native job flushing and client-buffer lifetimes unchanged.'},indent=2)+'\n')
