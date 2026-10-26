#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/generate_geometry_fast.py
python3 scripts/patch_geometry_fast.py
"${CC:-cc}" -std=gnu11 -O2 -fno-strict-aliasing -fwrapv -ffp-contract=off \
 -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 tests/geometry_fast.c \
 runtime/geometry_fast.c build/geometry-math-stage/reference.c -lm -o build/geometry-math-stage/promoted-selftest
build/geometry-math-stage/promoted-selftest --disabled
build/geometry-math-stage/promoted-selftest "$@"
