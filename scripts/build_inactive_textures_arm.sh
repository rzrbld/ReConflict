#!/usr/bin/env bash
# Requires staged renderer/header/facade from the reviewed local tests.
# Produces isolated ARM helpers only; never builds/deploys the game.
set -euo pipefail
cd "$(dirname "$0")/.."
directory=build/candidates/inactive-textures
[[ -f "$directory/d3d8_renderer.c" && -f "$directory/d3d8_renderer.h" && -f "$directory/d3d8.c" ]] || {
  echo 'Stage and test the renderer and target rollback candidates first' >&2; exit 1;
}
image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
  -v "$PWD:/src:ro" -v "$PWD/$directory:/out:rw" "$image" bash -eu -o pipefail -c '
    flags="-std=gnu11 -O3 -mcpu=cortex-a53 -I/src/build/sdk-include -I/src/build/sdk-include/SDL2 -I/usr/include/SDL2 -I/out -I/src/runtime"
    aarch64-linux-gnu-gcc $flags /src/tests/inactive_textures.c /out/d3d8_renderer.c \
      -Wl,--wrap=glActiveTexture,--wrap=glBindTexture,--wrap=glDrawArrays \
      -L/src/build/target-libs -Wl,--allow-shlib-undefined -lSDL2 -lGLESv2 -lm -o /out/inactive-textures.aarch64
    aarch64-linux-gnu-gcc $flags -ffunction-sections -fdata-sections -Wl,--gc-sections \
      -I/src/third_party/pcrecomp/runtime/win32hle -I/src/third_party/pcrecomp/runtime/recomp32 \
      -DDS_FACADE_SOURCE=\"/out/d3d8.c\" /src/tests/d3d8_target_rollback.c /out/d3d8_renderer.c \
      -L/src/build/target-libs -Wl,--allow-shlib-undefined -lSDL2 -lGLESv2 -lm -o /out/target-rollback.aarch64
  '
sha256sum "$directory/"*.aarch64 > "$directory/ARM-SHA256SUMS"
for name in inactive-textures target-rollback; do
  readelf -h "$directory/$name.aarch64" > "$directory/$name.elf-header.txt"
  readelf -d "$directory/$name.aarch64" > "$directory/$name.dynamic.txt"
done
