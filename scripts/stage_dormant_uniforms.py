#!/usr/bin/env python3
"""Stage a checked renderer-only candidate; never edit active runtime sources."""
from pathlib import Path
import difflib
import hashlib
import json

root=Path(__file__).resolve().parent.parent
source=root/'runtime/d3d8_renderer.c'
fixture=root/'tests/fixtures/d3d8_renderer_pre_dormant.c'
out=root/'build/candidates/dormant-uniforms'
out.mkdir(parents=True,exist_ok=True)
original=fixture.read_text()
if hashlib.sha256(original.encode()).hexdigest()!='7142dc1a2e8e932db4ff3ac61faa8d904d31e551220a9a0c2b64bb10e22dff17':
    raise SystemExit('Original renderer fixture hash mismatch')
staged=original
def replace(old,new):
    global staged
    if staged.count(old)!=1:
        raise SystemExit('Unexpected renderer source anchor: '+old[:100])
    staged=staged.replace(old,new)

replace(' D8GpuUploadCache *uploaded=&r->gpu_uploaded;float inv[9];\n',
''' D8GpuUploadCache *uploaded=&r->gpu_uploaded;
 /* The RHW branch consumes screen coordinates directly. Leave dormant
  * snapshots unchanged so the next XYZ draw uploads its actual state. */
 if(pos!=4){
 float inv[9];
''')
replace(' if(gpu_upload_changed(uploaded,3,uploaded->matrix[3],s->transform[16],64))',
''' }
 if(gpu_upload_changed(uploaded,3,uploaded->matrix[3],s->transform[16],64))''')
replace(' unsigned src[4]={s->rs[145],s->rs[147],s->rs[146],s->rs[148]};',
''' int lighting=pos!=4&&s->rs[137]&&(s->fvf&16);
 if(lighting){
 unsigned src[4]={s->rs[145],s->rs[147],s->rs[146],s->rs[148]};''')
replace(' small4i(small,SU_TEXFLAGS,u->texflags,',
''' }
 small4i(small,SU_TEXFLAGS,u->texflags,''')
replace(' if(gpu_upload_changed(uploaded,5,uploaded->material,s->material,sizeof s->material))',
''' /* Unlit XYZ and RHW draws cannot consume lighting uniforms. Crucially,
  * do not mark deferred values as uploaded when updating guest state. */
 if(lighting){
 if(gpu_upload_changed(uploaded,5,uploaded->material,s->material,sizeof s->material))''')
replace(' size_t vertex_bytes=((size_t)max-first+1)*stride;',
''' }
 size_t vertex_bytes=((size_t)max-first+1)*stride;''')
replace('    glUseProgram(program);raster(r,s,th);',
'''    /* gpu_prepare already made this program current before its uniforms. */
    if(!gpu)glUseProgram(program);raster(r,s,th);''')

if source.read_text() not in (original,staged):
    raise SystemExit('Active renderer differs from both checked baseline and candidate; review before restaging')

(out/'d3d8_renderer.c').write_text(staged)
(out/'original_renderer.c').write_text(original)
(out/'renderer.patch').write_text(''.join(difflib.unified_diff(
    original.splitlines(True),staged.splitlines(True),
    fromfile='a/runtime/d3d8_renderer.c',tofile='b/runtime/d3d8_renderer.c')))
(out/'manifest.json').write_text(json.dumps({
    'source':str(source.relative_to(root)),
    'baseline_fixture':str(fixture.relative_to(root)),
    'source_sha256':hashlib.sha256(original.encode()).hexdigest(),
    'candidate_sha256':hashlib.sha256(staged.encode()).hexdigest(),
    'scope':'RHW matrix/lighting deferral; unlit XYZ lighting deferral; duplicate GPU program bind'
},indent=2)+'\n')
print(out)
