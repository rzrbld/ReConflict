#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# Existing, inspected x86-host cross compiler. Override with an equivalent image.
image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
test -s build/target-libs/libSDL2.so
test -s build/target-libs/libGLESv2.so
mkdir -p build/probe
mkdir -p build/sdk-include/SDL2
# Reuse the real ARM SDL public configuration; never include x86 config headers.
sdk=${SDL_SDK_IMAGE:-sha256:cb0826a102ce4040137dcc702bfe30ccc8c153aa535a55e3897afb312db52702}
container=$(docker create --platform linux/arm64 "$sdk")
trap 'docker rm "$container" >/dev/null' EXIT
docker cp "$container:/usr/include/SDL2/SDL_config.h" build/sdk-include/SDL2/SDL_config.h
# Debian/Ubuntu SDL headers can use an architecture-specific indirection.
if grep -q _real_SDL_config build/sdk-include/SDL2/SDL_config.h; then
    docker cp "$container:/usr/include/aarch64-linux-gnu/SDL2/_real_SDL_config.h" build/sdk-include/SDL2/_real_SDL_config.h
fi
docker run --rm --network none --read-only --tmpfs /tmp \
    --user "$(id -u):$(id -g)" \
    -v "$PWD:/src:ro" -v "$PWD/build/probe:/out:rw" \
    --workdir /src "$image" sh -eu -c '
        aarch64-linux-gnu-gcc --version | head -1
        aarch64-linux-gnu-gcc -std=c11 -O2 -Wall -Wextra -Werror \
          -I/src/build/sdk-include/SDL2 -I/src/build/sdk-include -I/usr/include/SDL2 \
          probe/platform_probe.c probe/desert_pad.c probe/d3d8_gles.c runtime/paths.c -o /out/platform-probe.aarch64 \
          -L/src/build/target-libs -Wl,--allow-shlib-undefined \
          -Wl,-rpath-link,/src/build/target-libs -lSDL2 -lGLESv2 -lm
        aarch64-linux-gnu-gcc -std=c11 -O2 -Wall -Wextra -Werror \
          -I/src/build/sdk-include/SDL2 -I/src/build/sdk-include -I/usr/include/SDL2 \
          probe/launcher.c runtime/paths.c -o /out/desert-setup.aarch64 \
          -L/src/build/target-libs -Wl,--allow-shlib-undefined -lSDL2
    '
readelf -h build/probe/platform-probe.aarch64 | tee build/probe/elf-header.txt
readelf -d build/probe/platform-probe.aarch64 > build/probe/dynamic.txt
readelf -V build/probe/platform-probe.aarch64 > build/probe/versions.txt
sha256sum build/probe/platform-probe.aarch64 > build/probe/SHA256SUMS
