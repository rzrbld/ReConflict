#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=build/loading-background-tests
mkdir -p "$out"
proxy=build/render-thread-stage/wake-batch/render_thread_proxy.c
read -r -a cflags <<< "$(pkg-config --cflags sdl2 glesv2)"
read -r -a libs <<< "$(pkg-config --libs sdl2 glesv2)"
cc -std=gnu11 -O2 -Iprobe -Ibuild/render-thread-stage "${cflags[@]}" -c "$proxy" -o "$out/proxy-host.o"
mapfile -t names < <(nm -g --defined-only "$out/proxy-host.o" | awk '$3 ~ /^__wrap_/ {sub(/^__wrap_/,"",$3);print $3}' | sort -u)
wrappers=();for name in "${names[@]}";do wrappers+=("-Wl,--wrap=$name");done
cc -std=gnu11 -O2 -Iruntime -Iprobe "${cflags[@]}" tests/loading_background.c runtime/loading_background.c "$out/proxy-host.o" "${wrappers[@]}" "${libs[@]}" -lpthread -o "$out/test-host"
for mode in 0 1;do SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 DS_RENDER_THREAD=$mode "$out/test-host";done
if [[ ${1:-} == --cross ]];then
 image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" -v "$PWD/$out:/out:rw" --workdir /src "$image" bash -eu -o pipefail -c '
 flags="-std=gnu11 -O2 -mcpu=cortex-a53 -Iruntime -Iprobe -Ibuild/render-thread-stage -Ibuild/sdk-include -Ibuild/sdk-include/SDL2 -I/usr/include/SDL2"
 aarch64-linux-gnu-gcc $flags -c build/render-thread-stage/wake-batch/render_thread_proxy.c -o /out/proxy.o
 mapfile -t names < <(aarch64-linux-gnu-nm -g --defined-only /out/proxy.o | awk '\''$3 ~ /^__wrap_/ {sub(/^__wrap_/,"",$3);print $3}'\'' | sort -u)
 wrappers=();for name in "${names[@]}";do wrappers+=("-Wl,--wrap=$name");done
 aarch64-linux-gnu-gcc $flags tests/loading_background.c runtime/loading_background.c /out/proxy.o "${wrappers[@]}" -Lbuild/target-libs -Wl,--allow-shlib-undefined -lSDL2 -lGLESv2 -lpthread -ldl -o /out/test.aarch64
 '
fi
