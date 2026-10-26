#!/usr/bin/env python3
"""Isolated exact shader specialization for the observed smoke draw state."""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[1]
source = root / 'build/render-thread-game/src/d3d8_renderer.c'
data = source.read_bytes()
assert hashlib.sha256(data).hexdigest() == 'b8e26c819d172a6a3035eaabbc78e5e02333132911d60b69bc38965d2559a68b'
s = data.decode()
def replace(old, new):
    global s
    assert s.count(old) == 1, old[:100]
    s = s.replace(old, new, 1)

replace('static char draw_error[320];', '''static char draw_error[320];
/* Independent renderer caches are already supported by the raster/attribute/
 * texture owner tracking. No public struct or facade ABI change is needed. */
static D8Renderer smoke_renderer;
static D8Renderer *smoke_owner;
static uint64_t smoke_draws;
uint64_t ds_smoke_shader_draws(void){return smoke_draws;}
static const char smoke_fragment[]=
 "precision mediump float;varying vec4 dc;varying vec2 tc0;uniform sampler2D tex0;"
 "void main(){gl_FragColor=clamp(texture2D(tex0,tc0)*dc,0.,1.);}";
static int smoke_state(const D8State *s,const GLuint textures[2]){
    return textures[0] && !s->rs[15] && !s->rs[28] && !s->rs[29]
        && s->tss[0][1]==4 && s->tss[0][2]==2 && s->tss[0][3]==0
        && s->tss[0][4]==4 && s->tss[0][5]==2 && s->tss[0][6]==0
        && s->tss[1][1]==1;
}''')
replace('int d8_renderer_init(D8Renderer *r) {', 'static int init_fragment(D8Renderer *r,const char *fragment_override) {')
replace('    GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);',
        '    if(fragment_override)fs=fragment_override;\n    GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);')
replace('void d8_renderer_close(D8Renderer *r){', '''int d8_renderer_init(D8Renderer *r){
    int result=init_fragment(r,NULL);if(result)return result;
    const char *enabled=getenv("DS_FAST_PARTICLES");
    if(!smoke_owner && enabled && !strcmp(enabled,"1")){
        if(!init_fragment(&smoke_renderer,smoke_fragment)){
            smoke_owner=r;
            fprintf(stderr,"[d3d8] exact texture/diffuse particle shader enabled\\n");
        }else{
            d8_renderer_close(&smoke_renderer);
            fprintf(stderr,"[d3d8] particle shader unavailable; retaining generic path\\n");
        }
    }
    return 0;
}
void d8_renderer_close(D8Renderer *r){
    if(smoke_owner==r){smoke_owner=NULL;d8_renderer_close(&smoke_renderer);}
''')
anchor='''    draw_error[0]=0;unsigned n=d8_primitive_vertices(primitive,count),need=d8_fvf_stride(s->fvf);'''
replace(anchor, '''    if(r==smoke_owner && smoke_state(s,textures)){
        smoke_renderer.trim_vertex_prefix=r->trim_vertex_prefix;
        int result=d8_renderer_draw_native(&smoke_renderer,s,th,primitive,count,data,stride,vcount,indices,isize,base,textures,native);
        D8UploadStats uploads=d8_renderer_take_upload_stats(&smoke_renderer);
#define SMOKE_ADD(field) r->uploads.field+=uploads.field
        SMOKE_ADD(vertex_bytes);SMOKE_ADD(index_bytes);SMOKE_ADD(prefix_vertex_bytes);SMOKE_ADD(avoidable_prefix_bytes);
        SMOKE_ADD(vertex_uploads);SMOKE_ADD(index_uploads);SMOKE_ADD(trimmed_draws);SMOKE_ADD(native_vertex_draws);SMOKE_ADD(native_index_draws);
#undef SMOKE_ADD
        if(!result)smoke_draws++;
        return result;
    }
''' + anchor)
out = root / 'build/smoke-shader'
out.mkdir(parents=True, exist_ok=True)
(out / 'd3d8_renderer.c').write_text(s)
(out / 'inputs.json').write_text(json.dumps({
    'base': str(source.relative_to(root)), 'base_sha256': hashlib.sha256(data).hexdigest(),
    'output_sha256': hashlib.sha256(s.encode()).hexdigest(),
    'scope': 'Texture0 * diffuse, colour and alpha modulate; stage1 disabled; no alpha test, fog or specular. All other states retain generic shader. Opt-in DS_FAST_PARTICLES=1.',
}, indent=2)+'\n')
