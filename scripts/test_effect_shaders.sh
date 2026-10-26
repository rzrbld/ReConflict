#!/usr/bin/env bash
# Exercise all three specialization owners together, including fallbacks.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/stage_smoke_shader.py
python3 scripts/stage_fog_shader.py
python3 scripts/stage_glow_shader.py
out=${DS_EFFECT_TEST_OUTPUT:-build/effect-shader-tests}
renderer_source=${DS_EFFECT_RENDERER_SOURCE:-build/glow-shader/d3d8_renderer.c}
[[ "$out" =~ ^build/[a-zA-Z0-9_-]+$ && "$renderer_source" =~ ^build/[a-zA-Z0-9_/-]+[.]c$ && -f "$renderer_source" ]] || exit 2
export DS_EFFECT_TEST_OUTPUT="$out"
mkdir -p "$out"
python3 - <<'PY'
from pathlib import Path
import os
out=Path(os.environ["DS_EFFECT_TEST_OUTPUT"])
for name in ('smoke','fog','glow'):
 s=Path(f'probe/{name}_shader_test.c').read_text()
 anchor='D8Renderer renderer[2];'
 assert s.count(anchor)==1
 s=s.replace(anchor,anchor+'setenv("DS_FAST_PARTICLES","0",1);setenv("DS_FAST_FOG","0",1);setenv("DS_FAST_GLOW","0",1);')
 anchor='assert(!d8_renderer_init(renderer+1));'
 assert s.count(anchor)==1
 s=s.replace(anchor,'setenv("DS_FAST_PARTICLES","1",1);setenv("DS_FAST_FOG","1",1);setenv("DS_FAST_GLOW","1",1);'+anchor)
 if os.environ.get('DS_EFFECT_TEST_CHURN')=='1':
  anchor='for(int layer=0;layer<3;layer++){'
  assert s.count(anchor)==1
  s=s.replace(anchor,'''uint32_t original_spec=s.rs[29];
    for(int layer=0;layer<3;layer++){
     s.rs[29]=layer==1?1:original_spec;
     if(layer==1){
      glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
      d8_renderer_forget_texture(renderer+mode,texture);
      glDisableVertexAttribArray(0);d8_renderer_invalidate_attributes(renderer+mode);
      glBlendFunc(GL_ONE,GL_ONE);d8_renderer_invalidate_raster(renderer+mode);
     }''')
  s=s.replace('eligible?3u:0u','eligible?2u:0u')
 (out/f'{name}_test.c').write_text(s)
PY
proxy=${DS_EFFECT_PROXY_SOURCE:-build/render-thread-stage/client-reset-cacheline/render_thread_proxy.c}
read -r -a cflags <<< "$(pkg-config --cflags sdl2 glesv2)"
read -r -a libs <<< "$(pkg-config --libs sdl2 glesv2)"
cc -std=gnu11 -O2 -Iprobe -Ibuild/render-thread-stage "${cflags[@]}" -c "$proxy" -o "$out/proxy-host.o"
mapfile -t names < <(nm -g --defined-only "$out/proxy-host.o" | awk '$3 ~ /^__wrap_/ {sub(/^__wrap_/,"",$3);print $3}' | sort -u)
wrappers=();for name in "${names[@]}";do wrappers+=("-Wl,--wrap=$name");done
for name in smoke fog glow;do
 cc -std=gnu11 -O3 -fno-strict-aliasing -ffp-contract=off -Iprobe -Ibuild/render-thread-game/snapshot/runtime \
  "${cflags[@]}" "$out/${name}_test.c" "$renderer_source" runtime/d3d8_lights.c \
  "$out/proxy-host.o" "${wrappers[@]}" "${libs[@]}" -pthread -lm -o "$out/$name-host"
 timeout 60s env SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 "$out/$name-host" > "$out/$name-host.log" 2>&1
 tail -1 "$out/$name-host.log"
done
sha256sum "$proxy" "$renderer_source" "$out"/*_test.c runtime/d3d8_lights.c \
 build/render-thread-game/snapshot/runtime/d3d8_renderer.h > "$out/TEST-INPUT-SHA256SUMS"
if [[ ${1:-} == --cross ]];then
 image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -e EFFECT_RENDERER_SOURCE="$renderer_source" -e EFFECT_PROXY_SOURCE="$proxy" -v "$PWD:/src:ro" -v "$PWD/$out:/out:rw" --workdir /src "$image" bash -eu -o pipefail -c '
 flags="-std=gnu11 -O3 -mcpu=cortex-a53 -Iprobe -Ibuild/render-thread-stage -Ibuild/render-thread-game/snapshot/runtime -Ibuild/sdk-include -Ibuild/sdk-include/SDL2 -I/usr/include/SDL2"
 aarch64-linux-gnu-gcc $flags -O2 -c "$EFFECT_PROXY_SOURCE" -o /out/proxy.o
 mapfile -t names < <(aarch64-linux-gnu-nm -g --defined-only /out/proxy.o | awk '\''$3 ~ /^__wrap_/ {sub(/^__wrap_/,"",$3);print $3}'\'' | sort -u)
 wrappers=();for name in "${names[@]}";do wrappers+=("-Wl,--wrap=$name");done
 for name in smoke fog glow;do
  aarch64-linux-gnu-gcc $flags -fno-strict-aliasing -ffp-contract=off \
   /out/${name}_test.c "$EFFECT_RENDERER_SOURCE" runtime/d3d8_lights.c /out/proxy.o \
   "${wrappers[@]}" -Lbuild/target-libs -Wl,--allow-shlib-undefined -lSDL2 -lGLESv2 -lm -pthread -ldl -o /out/$name.aarch64
 done'
 sha256sum "$out"/*.aarch64 > "$out/SHA256SUMS"
fi
sha256sum -c "$out/TEST-INPUT-SHA256SUMS"
