#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/stage_dormant_uniforms.py
out=build/candidates/dormant-uniforms
python3 - <<'PY'
from pathlib import Path
import re
s=Path('tests/fixtures/d3d8_renderer_pre_dormant.c').read_text()
names=sorted(set(re.findall(r'\bd8_[A-Za-z0-9_]+',s)))
defines=''.join(f'#define {name} baseline_{name}\n' for name in names)
Path('build/candidates/dormant-uniforms/baseline_renderer.c').write_text(defines+s)
PY
"${CC:-cc}" -std=c11 -O3 -I/usr/include/SDL2 -Iruntime \
  tests/dormant_uniforms.c "$out/d3d8_renderer.c" "$out/baseline_renderer.c" \
  -Wl,--wrap=glUniformMatrix4fv,--wrap=glUniformMatrix3fv,--wrap=glUniform4fv,--wrap=glUniform3fv \
  -Wl,--wrap=glUniform1f,--wrap=glUniform1i,--wrap=glUniform4i,--wrap=glUseProgram \
  -lSDL2 -lGLESv2 -lm -o "$out/test"
SDL_VIDEODRIVER=offscreen "$out/test" | tee "$out/test.log"
"${CC:-cc}" -std=c11 -O3 -DDS_TEST_UNIFORM_COUNTS -DDS_TEST_SMALL_UNIFORM_COUNTS -I/usr/include/SDL2 -Iruntime \
  runtime/d3d8_gpu_selftest.c "$out/d3d8_renderer.c" \
  -Wl,--wrap=glUniformMatrix4fv,--wrap=glUniformMatrix3fv,--wrap=glUniform4fv \
  -Wl,--wrap=glUniform1i,--wrap=glUniform4i,--wrap=glUniform1f,--wrap=glUniform2f \
  -Wl,--wrap=glUniform3f,--wrap=glUniform4f,--wrap=glUniform3fv \
  -lSDL2 -lGLESv2 -lm -o "$out/parity"
SDL_VIDEODRIVER=offscreen "$out/parity" | tee "$out/parity.log"
python3 - <<'PY'
from pathlib import Path
import subprocess
directory=Path('build/candidates/dormant-uniforms')
applied=Path('runtime/d3d8_renderer.c').read_bytes()==(directory/'d3d8_renderer.c').read_bytes()
with (directory/'renderer.patch').open('rb') as patch:
    subprocess.run(['patch','-p1','--dry-run']+(['--reverse'] if applied else []),stdin=patch,check=True)
print('Active renderer: '+('candidate already integrated' if applied else 'original unchanged'))
PY
