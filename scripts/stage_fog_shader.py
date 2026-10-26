#!/usr/bin/env python3
"""Isolated linear-fog shader specialization layered on verified build39."""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[1]
source = root / 'build/smoke-shader/d3d8_renderer.c'
data = source.read_bytes()
assert hashlib.sha256(data).hexdigest() == '06394bf898c779db4e0a1b09ada3f166ce3ff6da4405528c47a3606bf8682d3c'
s = data.decode()
def replace(old, new):
    global s
    assert s.count(old) == 1, old[:100]
    s = s.replace(old, new, 1)

fragments = []
for alpha in ('t.a', 't.a*dc.a'):
    fragments.append('precision mediump float;varying vec4 dc;varying vec2 tc0;varying float fd;'
        'uniform sampler2D tex0;uniform int afunc;uniform float aref;uniform vec4 fogcolor;uniform vec3 fogparam;'
        'void main(){vec4 t=texture2D(tex0,tc0);vec4 cur=clamp(vec4(t.rgb*dc.rgb,'+alpha+'),0.,1.);'
        'if(afunc==7&&!(cur.a>=aref))discard;'
        'float f=(fogparam.y-fd)/max(.0001,fogparam.y-fogparam.x);'
        'cur.rgb=mix(fogcolor.rgb,cur.rgb,clamp(f,0.,1.));gl_FragColor=cur;}')
new = '''static D8Renderer fog_renderer[2];
static D8Renderer *fog_owner;
static int fog_ready[2];
static uint64_t fog_draws;
uint64_t ds_fog_shader_draws(void){return fog_draws;}
static const char *fog_fragment[2]={\n'''+',\n'.join(json.dumps(x) for x in fragments)+'''\n};
static int fog_state(const D8State *s,const GLuint textures[2]){
    if(!textures[0] || !s->rs[28] || s->rs[35]!=3 || s->rs[29]
       || (s->rs[15] && s->rs[25]!=7)
       || s->tss[0][1]!=4 || s->tss[0][2]!=2 || s->tss[0][3]!=0
       || s->tss[0][5]!=2 || s->tss[0][6]!=0 || s->tss[1][1]!=1)return -1;
    if(s->tss[0][4]==2)return 0;
    if(s->tss[0][4]==4)return 1;
    return -1;
}
'''
replace('static D8Renderer smoke_renderer;',new+'static D8Renderer smoke_renderer;')
replace('''    return 0;
}
void d8_renderer_close(D8Renderer *r){''','''    enabled=getenv("DS_FAST_FOG");
    if(!fog_owner && enabled && !strcmp(enabled,"1")){
        for(int i=0;i<2;i++){
            fog_ready[i]=!init_fragment(&fog_renderer[i],fog_fragment[i]);
            if(!fog_ready[i])d8_renderer_close(&fog_renderer[i]);
        }
        if(fog_ready[0] || fog_ready[1]){
            fog_owner=r;
            fprintf(stderr,"[d3d8] exact linear fog shaders enabled (%d,%d)\\n",fog_ready[0],fog_ready[1]);
        }
    }
    return 0;
}
void d8_renderer_close(D8Renderer *r){
    if(fog_owner==r){fog_owner=NULL;for(int i=0;i<2;i++){if(fog_ready[i])d8_renderer_close(&fog_renderer[i]);fog_ready[i]=0;}}''')
anchor='''    if(r==smoke_owner && smoke_state(s,textures)){'''
replace(anchor,'''    int fog_variant=r==fog_owner?fog_state(s,textures):-1;
    if(fog_variant>=0 && fog_ready[fog_variant]){
        D8Renderer *special=&fog_renderer[fog_variant];
        special->trim_vertex_prefix=r->trim_vertex_prefix;
        int result=d8_renderer_draw_native(special,s,th,primitive,count,data,stride,vcount,indices,isize,base,textures,native);
        D8UploadStats uploads=d8_renderer_take_upload_stats(special);
#define FOG_ADD(field) r->uploads.field+=uploads.field
        FOG_ADD(vertex_bytes);FOG_ADD(index_bytes);FOG_ADD(prefix_vertex_bytes);FOG_ADD(avoidable_prefix_bytes);
        FOG_ADD(vertex_uploads);FOG_ADD(index_uploads);FOG_ADD(trimmed_draws);FOG_ADD(native_vertex_draws);FOG_ADD(native_index_draws);
#undef FOG_ADD
        if(!result)fog_draws++;
        return result;
    }
'''+anchor)
out = root / 'build/fog-shader'
out.mkdir(parents=True, exist_ok=True)
(out/'d3d8_renderer.c').write_text(s)
(out/'inputs.json').write_text(json.dumps({
    'base':str(source.relative_to(root)), 'base_sha256':hashlib.sha256(data).hexdigest(),
    'output_sha256':hashlib.sha256(s.encode()).hexdigest(),
    'scope':'Explicit linear fog; texture0*diffuse colour; alpha texture or texture*diffuse; no specular; alpha test off or GE; stage1 disabled; DS_FAST_FOG=1 opt-in. Other states unchanged.'
},indent=2)+'\n')
