#!/bin/bash
# Independent real GLES correctness tests; no game or device operations.
set -euo pipefail
cd "$(dirname "$0")/.."
output=${DS_RT_TEST_OUTPUT:-build/render-thread-tests}
if [[ "${DS_RT_TEST_SANITIZE:-0}" == 1 ]];then output=$output/sanitized;fi
proxy=${DS_RT_PROXY_SOURCE:-probe/render_thread_proxy.c}
[[ -f "$proxy" ]] || { echo "Worker implementation unavailable: $proxy" >&2;exit 2; }
mkdir -p "$output"
python3 scripts/generate_render_proxy.py
read -r -a cflags <<< "$(pkg-config --cflags sdl2 glesv2)"
read -r -a libs <<< "$(pkg-config --libs sdl2 glesv2)"
if [[ "${DS_RT_TEST_SANITIZE:-0}" == 1 ]];then cflags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -g);export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0};fi
"${CC:-cc}" -std=gnu11 -O2 -Wall -Wextra -Iprobe -Ibuild/render-thread-stage "${cflags[@]}" -c "$proxy" -o "$output/proxy-host.o"
mapfile -t names < <(nm -g --defined-only "$output/proxy-host.o" | awk '$3 ~ /^__wrap_[A-Za-z_][A-Za-z0-9_]*$/ {sub(/^__wrap_/,"",$3);print $3}' | sort -u)
[[ ${#names[@]} -gt 0 ]] || { echo 'No public GL wrappers found' >&2;exit 2; }
wrappers=();for name in "${names[@]}";do wrappers+=("-Wl,--wrap=$name");done
"${CC:-cc}" -std=gnu11 -O2 -Wall -Wextra -Werror -Iprobe -Ibuild/render-thread-stage "${cflags[@]}" \
 probe/render_worker_regression.c "$output/proxy-host.o" "${wrappers[@]}" "${libs[@]}" -pthread -lm -o "$output/regression-host"
printf '%s\n' "${names[@]}" > "$output/linked-wrappers.txt"
timeout 30s env SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 "$output/regression-host" 2>&1 | tee "$output/host-normal.log"
timeout 30s env SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 "$output/regression-host" --error 2>&1 | tee "$output/host-error.log"

timeout 30s env SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 "$output/regression-host" --off 2>&1 | tee "$output/host-off.log"
