#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/target-rollback-tests
facade_source=${DS_TARGET_FACADE_SOURCE:-../runtime/d3d8.c}
"${CC:-cc}" -std=gnu11 -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -I/usr/include/SDL2 -Iruntime -Ithird_party/pcrecomp/runtime/win32hle -Ithird_party/pcrecomp/runtime/recomp32 \
  "-DDS_FACADE_SOURCE=\"$facade_source\"" tests/d3d8_target_rollback.c runtime/d3d8_renderer.c \
  -o build/target-rollback-tests/test -lSDL2 -lGLESv2 -lm
build/target-rollback-tests/test
