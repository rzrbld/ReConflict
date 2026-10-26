#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/game
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -Iruntime \
    tests/d3d8_lights.c runtime/d3d8_lights.c -o build/game/d3d8-lights-selftest
build/game/d3d8-lights-selftest
