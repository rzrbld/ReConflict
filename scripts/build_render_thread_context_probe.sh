#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/render-thread-probe
image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -v "$PWD:/src:ro" -v "$PWD/build/render-thread-probe:/out:rw" "$image" bash -eu -o pipefail -c '
 aarch64-linux-gnu-gcc -std=c11 -O2 -Wall -Wextra -Werror=implicit-function-declaration \
   -I/src/build/sdk-include -I/src/build/sdk-include/SDL2 -I/usr/include/SDL2 \
   /src/probe/render_thread_context.c -L/src/build/target-libs -Wl,--allow-shlib-undefined \
   -lSDL2 -lGLESv2 -o /out/render-thread-context.aarch64
 '
readelf -h build/render-thread-probe/render-thread-context.aarch64 > build/render-thread-probe/elf-header.txt
readelf -d build/render-thread-probe/render-thread-context.aarch64 > build/render-thread-probe/dynamic.txt
sha256sum build/render-thread-probe/render-thread-context.aarch64 > build/render-thread-probe/SHA256SUMS
