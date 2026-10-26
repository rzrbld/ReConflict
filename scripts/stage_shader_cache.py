#!/usr/bin/env python3
"""Reuse context state across exact shader programs, preserving uniform ownership."""
from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1];source=root/'build/glow-shader/d3d8_renderer.c';data=source.read_bytes()
assert hashlib.sha256(data).hexdigest()=='e32a3ef2521f0632e745ea48c0f86bf0ece1cbb24ba1370118e83b56f64c15cd'
s=data.decode()
def replace(old,new,count=1):
 global s
 assert s.count(old)==count,(s.count(old),old[:100])
 s=s.replace(old,new)
replace('static D8Renderer *attribute_owner,*texture_owner;', '''static D8Renderer *attribute_owner,*texture_owner;
/* These are GL-context/object state, independent of the current program.
 * Uniform caches stay per program. Producer owns all cache mutations. */
static D8TextureParameters context_texture_parameters[D8_TEXTURE_PARAMETER_SLOTS];
static uint32_t context_texture_epoch=1;''')
replace('static void next_texture_epoch(D8Renderer *r){if(++r->texture_epoch==0){memset(r->texture_parameters,0,sizeof r->texture_parameters);r->texture_epoch=1;}}',
'''static void next_texture_epoch(D8Renderer *r){(void)r;if(++context_texture_epoch==0){memset(context_texture_parameters,0,sizeof context_texture_parameters);context_texture_epoch=1;}}''')
replace('&r->texture_parameters[texture_slot(texture)]','&context_texture_parameters[texture_slot(texture)]',2)
replace('if(raster_owner!=r){c->valid=0;raster_owner=r;}',
'if(raster_owner!=r){if(raster_owner)*c=raster_owner->raster;else c->valid=0;raster_owner=r;}')
replace('if(attribute_owner!=r){memset(&r->attributes,0,sizeof r->attributes);attribute_owner=r;}',
'if(attribute_owner!=r){if(attribute_owner)r->attributes=attribute_owner->attributes;else memset(&r->attributes,0,sizeof r->attributes);attribute_owner=r;}')
replace('if(texture_owner!=r){next_texture_epoch(r);texture_owner=r;}','texture_owner=r;')
replace('if(p->texture!=texture||p->epoch!=r->texture_epoch){p->texture=texture;p->epoch=r->texture_epoch;p->valid=0;}',
'if(p->texture!=texture||p->epoch!=context_texture_epoch){p->texture=texture;p->epoch=context_texture_epoch;p->valid=0;}')
replace('static int init_fragment(D8Renderer *r,const char *fragment_override) {',
'''static int init_fragment(D8Renderer *r,const char *fragment_override) {
    next_texture_epoch(r); /* New renderer/context must not inherit stale GL names. */''')
out=root/'build/shader-cache';out.mkdir(parents=True,exist_ok=True)
(out/'d3d8_renderer.c').write_text(s)
(out/'inputs.json').write_text(json.dumps({'base':str(source.relative_to(root)),'base_sha256':hashlib.sha256(data).hexdigest(),'output_sha256':hashlib.sha256(s.encode()).hexdigest(),'scope':'Transfer last-known context raster/attribute state between shader renderers; shared texture parameter cache with explicit invalidation; program uniform caches unchanged.'},indent=2)+'\n')
