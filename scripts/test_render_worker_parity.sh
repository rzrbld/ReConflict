#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/generate_render_proxy.py
python3 scripts/stage_render_worker_parity.py
output=${DS_RT_TEST_OUTPUT:-build/render-thread-tests}
proxy=${DS_RT_PROXY_SOURCE:-probe/render_thread_proxy.c}
read -r -a cflags <<< "$(pkg-config --cflags sdl2 glesv2)"
read -r -a libs <<< "$(pkg-config --libs sdl2 glesv2)"
"${CC:-cc}" -std=gnu11 -O2 -Wall -Wextra -Werror -Iprobe -Ibuild/render-thread-stage "${cflags[@]}" -c "$proxy" -o "$output/proxy-parity-host.o"
mapfile -t names < <(nm -g --defined-only "$output/proxy-parity-host.o" | awk '$3 ~ /^__wrap_[A-Za-z_][A-Za-z0-9_]*$/ {sub(/^__wrap_/,"",$3);print $3}' | sort -u)
wrappers=();for name in "${names[@]}";do wrappers+=("-Wl,--wrap=$name");done
"${CC:-cc}" -std=gnu11 -O2 -fno-strict-aliasing -ffp-contract=off \
 -Ibuild/render-thread-game/snapshot/runtime -Iprobe -Ibuild/render-thread-stage "${cflags[@]}" \
 "$output/gpu_proxy_selftest.c" build/render-thread-game/src/d3d8_renderer.c "$output/parity-lights.c" "$output/proxy-parity-host.o" \
 "${wrappers[@]}" "${libs[@]}" -pthread -lm -o "$output/gpu-proxy-selftest-host"
timeout 60s env SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 "$output/gpu-proxy-selftest-host" 2>&1 | tee "$output/host-renderer-parity.log"
