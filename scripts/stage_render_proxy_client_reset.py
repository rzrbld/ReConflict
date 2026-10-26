#!/usr/bin/env python3
"""Stage authoritative client-history reset, separately from cursor changes."""
from pathlib import Path
import difflib,hashlib,json,shutil
root=Path(__file__).resolve().parent.parent
source=root/'probe/render_thread_proxy.c';original=source.read_text()
expected='044820509f391aa93abbdfcf9aaf6f9f31e135c969d9a96bb1102b0af54e56c7'
if hashlib.sha256(original.encode()).hexdigest()!=expected:raise SystemExit('Frozen proxy source changed; review first')
candidate=original
def replace(old,new):
 global candidate
 if candidate.count(old)!=1:raise SystemExit('Unexpected staging anchor '+old[:100])
 candidate=candidate.replace(old,new)
replace('static int native_es3;', '''static int native_es3;
static int client_trace_enabled,client_trace_count,client_reported_disabled;
static void client_trace(const char *reason,int index,uintptr_t pointer,int previous){
 if(!client_trace_enabled||client_trace_count>=32)return;
 client_trace_count++;
 fprintf(stderr,"[render-thread-client] reason=%s guest=%#x frame=%u sequence=%" PRIu64 " index=%d pointer=%#" PRIxPTR " previous=%d current=%d unknown=%d mask=%#" PRIx64 "\\n",reason,q.guest,q.frame,q.next_sequence,index,pointer,previous,proxy_client_ever,proxy_client_unknown,proxy_client_mask);
}''')
replace('uint64_t enabled;int client_seen;} UnpackState;',
        'uint64_t enabled;int client_seen,client_index,disabled_index;uintptr_t client_pointer,disabled_pointer;} UnpackState;')
replace('memset(s,0,sizeof *s);__real_glGetIntegerv(GL_UNPACK_ALIGNMENT,&s->alignment);',
        'memset(s,0,sizeof *s);s->client_index=s->disabled_index=-1;__real_glGetIntegerv(GL_UNPACK_ALIGNMENT,&s->alignment);')
replace('''if(!s->attrib_buffer[i]){void *pointer=NULL;__real_glGetVertexAttribPointerv(i,GL_VERTEX_ATTRIB_ARRAY_POINTER,&pointer);if(enabled||pointer)s->client_seen=1;}''',
'''if(!s->attrib_buffer[i]){
   void *pointer=NULL;__real_glGetVertexAttribPointerv(i,GL_VERTEX_ATTRIB_ARRAY_POINTER,&pointer);
   if(enabled){s->client_seen=1;if(s->client_index<0){s->client_index=i;s->client_pointer=(uintptr_t)pointer;}}
   else if(pointer&&s->disabled_index<0){s->disabled_index=i;s->disabled_pointer=(uintptr_t)pointer;}
  }''')
replace('''static void unpack_restore_shadow(const UnpackState *s){
 proxy_unpack_alignment''','''static void unpack_restore_shadow(const UnpackState *s){
 int previous_client=proxy_client_ever,previous_unknown=proxy_client_unknown;
 proxy_unpack_alignment''')
replace(' if(s->client_seen)proxy_client_ever=1;',
''' /* This reply follows a fully drained blocking job/startup snapshot. Only
  * ENABLED client arrays can borrow memory during a draw. Disabled descriptors
  * retain buffer0 in the shadow, so a later enable still forces synchronous
  * execution. Never clear history/unknown from speculative setters alone. */
 proxy_client_ever=s->client_seen!=0;''')
replace(''' for(unsigned i=0;i<64;i++){proxy_attrib_buffer[i]=(GLuint)s->attrib_buffer[i];if(!proxy_attrib_buffer[i])proxy_client_mask|=proxy_enabled_mask&(UINT64_C(1)<<i);}
}''',''' for(unsigned i=0;i<64;i++){proxy_attrib_buffer[i]=(GLuint)s->attrib_buffer[i];if(!proxy_attrib_buffer[i])proxy_client_mask|=proxy_enabled_mask&(UINT64_C(1)<<i);}
 if(previous_client!=proxy_client_ever||previous_unknown!=proxy_client_unknown)client_trace(proxy_client_ever?"snapshot-active-client":"authoritative-reset",s->client_index,s->client_pointer,previous_client);
 if(s->disabled_index>=0&&!client_reported_disabled){client_trace("ignored-disabled-offset",s->disabled_index,s->disabled_pointer,previous_client);client_reported_disabled=1;}
}''')
replace(' if(!proxy_array_buffer)proxy_client_ever=1;',
''' if(!proxy_array_buffer){int previous=proxy_client_ever;proxy_client_ever=1;if(!previous)client_trace("producer-client-pointer",(int)index,(uintptr_t)pointer,previous);}''')
replace(' memset(&q,0,sizeof q);q.window=window;q.context=context;q.producer=SDL_ThreadID();',
''' memset(&q,0,sizeof q);q.window=window;q.context=context;q.producer=SDL_ThreadID();
 const char *trace=getenv("DS_RT_TRACE_CLIENT");client_trace_enabled=trace&&!strcmp(trace,"1");client_trace_count=client_reported_disabled=0;''')
out=root/'build/render-thread-stage/client-reset';out.mkdir(parents=True,exist_ok=True)
(out/'render_thread_proxy.c').write_text(candidate)
for name in ['render_proxy_ops.inc','render_proxy_wrappers.inc']:shutil.copyfile(root/'build/render-thread-stage'/name,out/name)
shutil.copyfile(root/'probe/render_thread_proxy.h',out/'render_thread_proxy.h')
(out/'guard-reset.patch').write_text(''.join(difflib.unified_diff(original.splitlines(True),candidate.splitlines(True),fromfile='a/probe/render_thread_proxy.c',tofile='b/probe/render_thread_proxy.c')))
(out/'manifest.json').write_text(json.dumps({'baseline_sha256':expected,'candidate_sha256':hashlib.sha256(candidate.encode()).hexdigest(),'files':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(out.glob('*')) if p.suffix in ('.c','.h','.inc')},'scope':'authoritative client-history reset+bounded diagnostics only; no cacheline/cursor changes'},indent=2)+'\n')
print(out)
