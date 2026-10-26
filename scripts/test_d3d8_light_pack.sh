#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/light-pack-tests
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -Iruntime \
  tests/d3d8_light_pack.c tests/fixtures/d3d8_light_pack_baseline.c runtime/d3d8_lights.c \
  -o build/light-pack-tests/differential
build/light-pack-tests/differential
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -Iruntime \
  tests/d3d8_lights.c runtime/d3d8_lights.c -o build/light-pack-tests/state-regression
build/light-pack-tests/state-regression
