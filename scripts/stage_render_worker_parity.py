#!/usr/bin/env python3
"""Stage original renderer parity test through proxy; omit only immediate-error recovery."""
from pathlib import Path
import hashlib,json,os
root=Path(__file__).resolve().parents[1];out=root/os.environ.get('DS_RT_TEST_OUTPUT','build/render-thread-tests');out.mkdir(parents=True,exist_ok=True)
path=root/'runtime/d3d8_gpu_selftest.c';source=path.read_text()
assert hashlib.sha256(source.encode()).hexdigest()=='e5a06f9c6e07f13f338800643d3f4fe24cb128569ed9283633a198ef572bc3de'
def replace(old,new):
 global source
 assert source.count(old)==1,old
 source=source.replace(old,new,1)
replace('#include "d3d8_renderer.h"','#include "d3d8_renderer.h"\n#include "render_thread_proxy.h"')
replace('if(!ctx)return 2;','if(!ctx)return 2;setenv("DS_RENDER_THREAD","1",1);if(ds_rt_start(w,ctx)||!ds_rt_active())return 2;')
a=source.index(' /* A failed GL submission invalidates all uniform snapshots')
b=source.index(' printf("GL_error_cache_invalidation_and_recovery=%s\\n",fail?"FAIL":"PASS");',a)+len(' printf("GL_error_cache_invalidation_and_recovery=%s\\n",fail?"FAIL":"PASS");')
omitted=source[a:b]
source=source[:a]+' printf("GL_error_cache_invalidation_and_recovery=OMITTED (immediate-error contract; deferred failure tested separately)\\n");'+source[b:]
replace(' d8_renderer_close(r);d8_renderer_close(r+1);SDL_GL_DeleteContext(ctx);',' if(ds_rt_stop()){const DsRtError *e=ds_rt_error();fprintf(stderr,"proxy stop failed: %s GL=%#x\\n",e?e->api:"unknown",e?e->code:0);return 2;}d8_renderer_close(r);d8_renderer_close(r+1);SDL_GL_DeleteContext(ctx);')
(out/'gpu_proxy_selftest.c').write_text(source)
lights=root/'runtime/d3d8_lights.c';(out/'parity-lights.c').write_bytes(lights.read_bytes())
paths=[path,root/'build/render-thread-game/src/d3d8_renderer.c',root/'build/render-thread-game/snapshot/runtime/d3d8_renderer.h',lights]
manifest={'inputs':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},'omitted_original_block_sha256':hashlib.sha256(omitted.encode()).hexdigest(),'omission':'Only original immediate GL-error/cache-invalidation/recovery block; deferred errors tested separately','source':str((out/'gpu_proxy_selftest.c').relative_to(root))}
(out/'parity-inputs.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest,indent=2))
