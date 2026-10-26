#!/usr/bin/env python3
"""Stage the cursor-layout experiment without editing the frozen proxy."""
from pathlib import Path
import difflib,hashlib,json,shutil
root=Path(__file__).resolve().parent.parent
source=root/'probe/render_thread_proxy.c';original=source.read_text()
expected='044820509f391aa93abbdfcf9aaf6f9f31e135c969d9a96bb1102b0af54e56c7'
if hashlib.sha256(original.encode()).hexdigest()!=expected:raise SystemExit('Frozen proxy source changed; review before staging')
candidate=original
def replace(old,new):
 global candidate
 if candidate.count(old)!=1:raise SystemExit('Unexpected staging anchor '+old[:80])
 candidate=candidate.replace(old,new)
replace('''static struct {
 unsigned char *arena;uint64_t head,next_sequence;_Atomic uint64_t published,consumed,completed;
 _Atomic int active,stopping,failed;SDL_mutex *mutex;SDL_cond *work,*space;SDL_sem *ready;SDL_Thread *thread;
 SDL_Window *window;SDL_GLContext context;SDL_threadID producer;uint32_t guest,frame;DsRtError error;
} q;''','''static struct QueueState {
 unsigned char *arena;
 /* Producer-private cursors must not share a line with worker publications. */
 _Alignas(64) uint64_t head,next_sequence,cached_consumed;uint32_t guest,frame;
 _Alignas(64) _Atomic uint64_t published;
 _Alignas(64) _Atomic uint64_t consumed,completed;
 _Alignas(64) _Atomic int active,stopping,failed;
 SDL_mutex *mutex;SDL_cond *work,*space;SDL_sem *ready;SDL_Thread *thread;
 SDL_Window *window;SDL_GLContext context;SDL_threadID producer;DsRtError error;
} q;
_Static_assert(offsetof(struct QueueState,head)%64==0,"producer cursor alignment");
_Static_assert(offsetof(struct QueueState,published)%64==0,"publication cursor alignment");
_Static_assert(offsetof(struct QueueState,consumed)%64==0,"consumer cursor alignment");''')
replace('''static void wait_space(size_t bytes){
 if(q.head-atomic_load_explicit(&q.consumed,memory_order_acquire)+bytes>CAPACITY){
  publish();SDL_LockMutex(q.mutex);
  while(q.head-atomic_load_explicit(&q.consumed,memory_order_acquire)+bytes>CAPACITY)SDL_CondWait(q.space,q.mutex);
  SDL_UnlockMutex(q.mutex);
 }
}''','''static void wait_space(size_t bytes){
 /* A stale tail only underestimates free space. Refresh it on a capacity
  * boundary, not for every scalar command, retaining acquire before reuse. */
 if(q.head-q.cached_consumed+bytes<=CAPACITY)return;
 q.cached_consumed=atomic_load_explicit(&q.consumed,memory_order_acquire);
 if(q.head-q.cached_consumed+bytes<=CAPACITY)return;
 publish();SDL_LockMutex(q.mutex);
 for(;;){
  q.cached_consumed=atomic_load_explicit(&q.consumed,memory_order_acquire);
  if(q.head-q.cached_consumed+bytes<=CAPACITY)break;
  SDL_CondWait(q.space,q.mutex);
 }
 SDL_UnlockMutex(q.mutex);
}''')
out=root/'build/render-thread-stage/cacheline';out.mkdir(parents=True,exist_ok=True)
(out/'render_thread_proxy.c').write_text(candidate)
for name in ['render_proxy_ops.inc','render_proxy_wrappers.inc']:
 shutil.copyfile(root/'build/render-thread-stage'/name,out/name)
shutil.copyfile(root/'probe/render_thread_proxy.h',out/'render_thread_proxy.h')
(out/'cursor.patch').write_text(''.join(difflib.unified_diff(original.splitlines(True),candidate.splitlines(True),fromfile='a/probe/render_thread_proxy.c',tofile='b/probe/render_thread_proxy.c')))
(out/'manifest.json').write_text(json.dumps({'baseline_sha256':expected,'candidate_sha256':hashlib.sha256(candidate.encode()).hexdigest(),'files':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(out.glob('*')) if p.suffix in ('.c','.h','.inc')},'changes':['Separate64-byte producer, publication and consumer cursor lines','Producer caches consumed cursor until a capacity boundary; acquire still required before reusing memory'],'scope':'staged-only, not a gameplay/FPS result'},indent=2)+'\n')
print(out)
