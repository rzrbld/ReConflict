#!/usr/bin/env python3
"""Exact alpha-from-texture glow shader, layered on the tested fog candidate."""
from pathlib import Path
import hashlib
import json
root=Path(__file__).resolve().parents[1]
source=root/'build/fog-shader/d3d8_renderer.c'
data=source.read_bytes()
assert hashlib.sha256(data).hexdigest()=='d2d35ca7b0006d612c0b0ec4b41989be6f92029b4c7cbad2b46bbffdc70654c5'
s=data.decode()
def replace(old,new):
 global s
 assert s.count(old)==1,old[:100]
 s=s.replace(old,new,1)
replace('static D8Renderer smoke_renderer;', '''static D8Renderer glow_renderer;
static D8Renderer *glow_owner;
static uint64_t glow_draws;
uint64_t ds_glow_shader_draws(void){return glow_draws;}
static const char glow_fragment[]=
 "precision mediump float;varying vec4 dc;varying vec2 tc0;uniform sampler2D tex0;"
 "void main(){vec4 t=texture2D(tex0,tc0);gl_FragColor=clamp(vec4(t.rgb*dc.rgb,t.a),0.,1.);}";
static int glow_state(const D8State *s,const GLuint textures[2]){
    return textures[0] && !s->rs[15] && !s->rs[28] && !s->rs[29]
        && s->tss[0][1]==4 && s->tss[0][2]==2 && s->tss[0][3]==0
        && s->tss[0][4]==2 && s->tss[0][5]==2 && s->tss[0][6]==0
        && s->tss[1][1]==1;
}
static D8Renderer smoke_renderer;''')
replace('''    return 0;
}
void d8_renderer_close(D8Renderer *r){''','''    enabled=getenv("DS_FAST_GLOW");
    if(!glow_owner && enabled && !strcmp(enabled,"1")){
        if(!init_fragment(&glow_renderer,glow_fragment)){
            glow_owner=r;
            fprintf(stderr,"[d3d8] exact texture-alpha glow shader enabled\\n");
        }else d8_renderer_close(&glow_renderer);
    }
    return 0;
}
void d8_renderer_close(D8Renderer *r){
    if(glow_owner==r){glow_owner=NULL;d8_renderer_close(&glow_renderer);}''')
anchor='''    if(r==smoke_owner && smoke_state(s,textures)){'''
replace(anchor,'''    if(r==glow_owner && glow_state(s,textures)){
        glow_renderer.trim_vertex_prefix=r->trim_vertex_prefix;
        int result=d8_renderer_draw_native(&glow_renderer,s,th,primitive,count,data,stride,vcount,indices,isize,base,textures,native);
        D8UploadStats uploads=d8_renderer_take_upload_stats(&glow_renderer);
#define GLOW_ADD(field) r->uploads.field+=uploads.field
        GLOW_ADD(vertex_bytes);GLOW_ADD(index_bytes);GLOW_ADD(prefix_vertex_bytes);GLOW_ADD(avoidable_prefix_bytes);
        GLOW_ADD(vertex_uploads);GLOW_ADD(index_uploads);GLOW_ADD(trimmed_draws);GLOW_ADD(native_vertex_draws);GLOW_ADD(native_index_draws);
#undef GLOW_ADD
        if(!result)glow_draws++;
        return result;
    }
'''+anchor)
out=root/'build/glow-shader';out.mkdir(parents=True,exist_ok=True)
(out/'d3d8_renderer.c').write_text(s)
(out/'inputs.json').write_text(json.dumps({'base':str(source.relative_to(root)),'base_sha256':hashlib.sha256(data).hexdigest(),'output_sha256':hashlib.sha256(s.encode()).hexdigest(),'scope':'Texture*diffuse RGB, texture alpha, no alpha test/fog/specular, stage1 disabled. DS_FAST_GLOW=1 opt-in; retains independent smoke/fog paths.'},indent=2)+'\n')
