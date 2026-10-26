#!/usr/bin/env bash
# Stage only: do not edit active renderer or build/deploy the game.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/candidates/inactive-textures
python3 - <<'PY'
from pathlib import Path
import difflib,hashlib,json
source=Path('runtime/d3d8_renderer.c');s=Path('tests/fixtures/d3d8_renderer_pre_inactive_textures.c').read_text()
if hashlib.sha256(s.encode()).hexdigest()!='ef6c4f7a9584fd4a11d4750c04223fba8fc1d9fac9be534bba7b3b0dd171efed':raise SystemExit('Original fixture hash mismatch')
old='GLuint texture=(unsigned)t<stages?textures[t]:0;glActiveTexture(GL_TEXTURE0+t);glBindTexture(GL_TEXTURE_2D,texture);if(texture)texture_parameters(r,texture,s->tss[t]);'
new='GLuint texture=(unsigned)t<stages?textures[t]:0;if(texture||!native||!native->default_framebuffer){glActiveTexture(GL_TEXTURE0+t);glBindTexture(GL_TEXTURE_2D,texture);if(texture)texture_parameters(r,texture,s->tss[t]);}'
if s.count(old)!=1:raise SystemExit('Unexpected renderer source: review staging anchor')
candidate=s.replace(old,new);out=Path('build/candidates/inactive-textures')
if source.read_text() not in (s,candidate):raise SystemExit('Active renderer differs from baseline and candidate')
header=Path('runtime/d3d8_renderer.h').read_text()
field='    size_t vertex_offset,index_offset,vertex_bytes,index_bytes;\n'
addition='    /* True only when the caller knows the CURRENT native framebuffer is0.\n     * Unknown/offscreen targets retain inactive sampler clearing to prevent\n     * texture/FBO feedback even with conditionally disabled sampling. */\n    int default_framebuffer;\n'
if addition in header:oldheader=header.replace(addition,'');newheader=header
else:
    if header.count(field)!=1:raise SystemExit('Unexpected native buffer context layout')
    oldheader=header;newheader=header.replace(field,field+addition)
(out/'d3d8_renderer.h').write_text(newheader)
(out/'original_renderer.c').write_text(s);(out/'d3d8_renderer.c').write_text(candidate)
(out/'renderer.patch').write_text(''.join(difflib.unified_diff(s.splitlines(True),candidate.splitlines(True),fromfile='a/runtime/d3d8_renderer.c',tofile='b/runtime/d3d8_renderer.c'))+''.join(difflib.unified_diff(oldheader.splitlines(True),newheader.splitlines(True),fromfile='a/runtime/d3d8_renderer.h',tofile='b/runtime/d3d8_renderer.h')))
(out/'manifest.json').write_text(json.dumps({'source_sha256':hashlib.sha256(s.encode()).hexdigest(),'candidate_sha256':hashlib.sha256(candidate.encode()).hexdigest()},indent=2)+'\n')
PY
out=build/candidates/inactive-textures
"${CC:-cc}" -std=c11 -O3 -I/usr/include/SDL2 -I"$out" -Iruntime \
  tests/inactive_textures.c "$out/d3d8_renderer.c" \
  -Wl,--wrap=glActiveTexture,--wrap=glBindTexture,--wrap=glDrawArrays -lSDL2 -lGLESv2 -lm -o "$out/test"
SDL_VIDEODRIVER=offscreen DS_GPU_VERTEX=0 "$out/test" | tee "$out/cpu.log"
SDL_VIDEODRIVER=offscreen DS_GPU_VERTEX=1 "$out/test" | tee "$out/gpu.log"
"${CC:-cc}" -std=c11 -O3 -I/usr/include/SDL2 -I"$out" -Iruntime \
  runtime/d3d8_gpu_selftest.c "$out/d3d8_renderer.c" \
  -lSDL2 -lGLESv2 -lm -o "$out/parity"
SDL_VIDEODRIVER=offscreen "$out/parity" | tee "$out/parity.log"
python3 - <<'PY'
from pathlib import Path
import subprocess
d=Path('build/candidates/inactive-textures')
applied=Path('runtime/d3d8_renderer.c').read_bytes()==(d/'d3d8_renderer.c').read_bytes()
with (d/'renderer.patch').open('rb') as p:subprocess.run(['patch','-p1','--dry-run']+(['--reverse'] if applied else []),stdin=p,check=True)
print('Active renderer: '+('candidate already promoted' if applied else 'unchanged baseline'))
PY
