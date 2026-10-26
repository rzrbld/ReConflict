#!/usr/bin/env bash
# Build only two isolated renderer tests. No game objects/executable/deployment.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/stage_dormant_uniforms.py
python3 - <<'PY'
from pathlib import Path
import re
s=Path('tests/fixtures/d3d8_renderer_pre_dormant.c').read_text()
defines=''.join(f'#define {name} baseline_{name}\n' for name in sorted(set(re.findall(r'\bd8_[A-Za-z0-9_]+',s))))
Path('build/candidates/dormant-uniforms/baseline_renderer.c').write_text(defines+s)
PY
image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
docker run --rm --network none --read-only --tmpfs /tmp \
  --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" \
  -v "$PWD/build/candidates/dormant-uniforms:/out:rw" --workdir /src \
  "$image" bash -eu -o pipefail -c '
    flags="-std=c11 -O3 -g -mcpu=cortex-a53 -I/src/build/sdk-include -I/src/build/sdk-include/SDL2 -I/usr/include/SDL2 -I/src/runtime"
    aarch64-linux-gnu-gcc $flags /src/tests/dormant_uniforms.c /out/d3d8_renderer.c /out/baseline_renderer.c \
      -Wl,--wrap=glUniformMatrix4fv,--wrap=glUniformMatrix3fv,--wrap=glUniform4fv,--wrap=glUniform3fv \
      -Wl,--wrap=glUniform1f,--wrap=glUniform1i,--wrap=glUniform4i,--wrap=glUseProgram \
      -L/src/build/target-libs -Wl,--allow-shlib-undefined -lSDL2 -lGLESv2 -lm -o /out/dormant-uniforms.aarch64
    aarch64-linux-gnu-gcc $flags -DDS_TEST_UNIFORM_COUNTS -DDS_TEST_SMALL_UNIFORM_COUNTS \
      /src/runtime/d3d8_gpu_selftest.c /out/d3d8_renderer.c \
      -Wl,--wrap=glUniformMatrix4fv,--wrap=glUniformMatrix3fv,--wrap=glUniform4fv \
      -Wl,--wrap=glUniform1i,--wrap=glUniform4i,--wrap=glUniform1f,--wrap=glUniform2f \
      -Wl,--wrap=glUniform3f,--wrap=glUniform4f,--wrap=glUniform3fv \
      -L/src/build/target-libs -Wl,--allow-shlib-undefined -lSDL2 -lGLESv2 -lm -o /out/parity.aarch64
  '
for name in dormant-uniforms parity; do
  readelf -h "build/candidates/dormant-uniforms/$name.aarch64" > "build/candidates/dormant-uniforms/$name.elf-header.txt"
  readelf -d "build/candidates/dormant-uniforms/$name.aarch64" > "build/candidates/dormant-uniforms/$name.dynamic.txt"
done
sha256sum build/candidates/dormant-uniforms/*.aarch64 > build/candidates/dormant-uniforms/ARM-SHA256SUMS
