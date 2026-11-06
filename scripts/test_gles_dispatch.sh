#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/portable-gles-tests
read -r -a cflags <<< "$(pkg-config --cflags sdl2)"
read -r -a libs <<< "$(pkg-config --libs sdl2)"
cc -std=gnu11 -O2 -Wall -Wextra -Werror -Iprobe "${cflags[@]}" runtime/gles_dispatch.c tests/gles_dispatch.c "${libs[@]}" -o build/portable-gles-tests/test-host
SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-offscreen} build/portable-gles-tests/test-host
if readelf -d build/portable-gles-tests/test-host | grep -E 'NEEDED.*(libmali|libGLES|libEGL)'; then
 echo 'Unexpected graphics dependency' >&2; exit 1
fi
