#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/stage_glow_shader.py
out=build/glow-shader
proxy=build/render-thread-stage/client-reset-cacheline/render_thread_proxy.c
read -r -a cflags <<< "$(pkg-config --cflags sdl2 glesv2)"
read -r -a libs <<< "$(pkg-config --libs sdl2 glesv2)"
cc -std=gnu11 -O2 -Iprobe -Ibuild/render-thread-stage "${cflags[@]}" -c "$proxy" -o "$out/proxy-host.o"
mapfile -t names < <(nm -g --defined-only "$out/proxy-host.o" | awk '$3 ~ /^__wrap_/ {sub(/^__wrap_/,"",$3); print $3}' | sort -u)
wrappers=();for name in "${names[@]}"; do wrappers+=("-Wl,--wrap=$name"); done
cc -std=gnu11 -O3 -fno-strict-aliasing -ffp-contract=off -Iprobe -Ibuild/render-thread-game/snapshot/runtime \
 "${cflags[@]}" probe/glow_shader_test.c "$out/d3d8_renderer.c" runtime/d3d8_lights.c \
 "$out/proxy-host.o" "${wrappers[@]}" "${libs[@]}" -pthread -lm -o "$out/test-host"
timeout 60s env SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 "$out/test-host" > "$out/host-test.log" 2>&1
cat "$out/host-test.log"
sha256sum "$proxy" "$out/d3d8_renderer.c" probe/glow_shader_test.c runtime/d3d8_lights.c \
 build/render-thread-game/snapshot/runtime/d3d8_renderer.h > "$out/TEST-INPUT-SHA256SUMS"
if [[ ${1:-} == --cross ]]; then
 image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
  -v "$PWD:/src:ro" -v "$PWD/$out:/out:rw" --workdir /src "$image" bash -eu -o pipefail -c '
   flags="-std=gnu11 -O3 -mcpu=cortex-a53 -Iprobe -Ibuild/render-thread-stage -Ibuild/render-thread-game/snapshot/runtime -Ibuild/sdk-include -Ibuild/sdk-include/SDL2 -I/usr/include/SDL2"
   aarch64-linux-gnu-gcc $flags -O2 -c build/render-thread-stage/client-reset-cacheline/render_thread_proxy.c -o /out/proxy.o
   mapfile -t names < <(aarch64-linux-gnu-nm -g --defined-only /out/proxy.o | awk '\''$3 ~ /^__wrap_/ {sub(/^__wrap_/,"",$3);print $3}'\'' | sort -u)
   wrappers=();for name in "${names[@]}";do wrappers+=("-Wl,--wrap=$name");done
   aarch64-linux-gnu-gcc $flags -fno-strict-aliasing -ffp-contract=off \
    probe/glow_shader_test.c /out/d3d8_renderer.c runtime/d3d8_lights.c /out/proxy.o \
    "${wrappers[@]}" -Lbuild/target-libs -Wl,--allow-shlib-undefined -lSDL2 -lGLESv2 -lm -pthread -ldl \
    -o /out/test.aarch64
  '
 sha256sum "$out/test.aarch64" > "$out/SHA256SUMS"
fi
sha256sum -c "$out/TEST-INPUT-SHA256SUMS"
