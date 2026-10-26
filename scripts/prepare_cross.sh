#!/usr/bin/env bash
# Public toolchain setup. Existing firmware link inputs are preserved.
set -euo pipefail
cd "$(dirname "$0")/.."
image=${CROSS_IMAGE:-reconflict-cross:bookworm}
docker build -f packaging/Dockerfile.cross -t "$image" .
mkdir -p build/sdk-include/SDL2 build/target-libs
docker run --rm --network none --read-only --tmpfs /tmp \
  --user "$(id -u):$(id -g)" -v "$PWD/build/sdk-include:/sdk:rw" \
  -v "$PWD/build/target-libs:/libs:rw" "$image" sh -eu -c '
    cp /usr/include/SDL2/SDL_config.h /sdk/SDL2/
    cp /usr/include/aarch64-linux-gnu/SDL2/_real_SDL_config.h /sdk/SDL2/
    cp /usr/include/SDL2/SDL_ttf.h /sdk/SDL2/
    for name in libSDL2.so libSDL2_ttf.so libGLESv2.so; do
      test -f /libs/$name || cp -L /usr/lib/aarch64-linux-gnu/$name /libs/$name
    done
  '
echo "Toolchain prepared. Set CROSS_IMAGE=$image for subsequent builds."
