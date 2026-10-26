#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/generate_physics_fast.py
python3 scripts/patch_physics_fast.py
python3 scripts/prepare_physics_tests.py
"${CC:-cc}" -std=gnu11 -O2 -fno-strict-aliasing -fwrapv -ffp-contract=off \
 -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 tests/physics_fast.c \
 runtime/physics_fast.c build/physics-stage/full-native.c build/physics-stage/full-reference.c \
 build/physics-stage/precise-callees.c -lm -o build/physics-stage/promoted-full-selftest
DS_FAST_MATH=1 build/physics-stage/promoted-full-selftest
DS_FAST_MATH=0 build/physics-stage/promoted-full-selftest
