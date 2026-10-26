#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
proxy=${DS_RT_PROXY_SOURCE:-build/render-thread-stage/client-reset/render_thread_proxy.c}
output=${DS_RT_TEST_OUTPUT:-build/render-thread-stage/client-reset/guard-tests}
[[ "${DS_RT_TEST_SANITIZE:-0}" == 1 ]] && output=$output/sanitized
mkdir -p "$output"
read -r -a cflags <<< "$(pkg-config --cflags sdl2 glesv2)"
read -r -a libs <<< "$(pkg-config --libs sdl2 glesv2)"
if [[ "${DS_RT_TEST_SANITIZE:-0}" == 1 ]];then cflags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -g);export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0};fi
"${CC:-cc}" -std=gnu11 -O2 -Wall -Wextra -Werror -Iprobe -Ibuild/render-thread-stage "${cflags[@]}" -c "$proxy" -o "$output/proxy.o"
mapfile -t names < <(nm -g --defined-only "$output/proxy.o" | awk '$3 ~ /^__wrap_[A-Za-z_][A-Za-z0-9_]*$/ {sub(/^__wrap_/,"",$3);print $3}' | sort -u)
wrappers=();for name in "${names[@]}";do wrappers+=("-Wl,--wrap=$name");done
"${CC:-cc}" -std=gnu11 -O2 -Wall -Wextra -Werror -Iprobe "${cflags[@]}" probe/render_worker_guard_reset.c "$output/proxy.o" "${wrappers[@]}" "${libs[@]}" -pthread -lm -o "$output/guard-reset"
timeout 30s env SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 "$output/guard-reset" 2>&1 | tee "$output/guard-reset.log"
