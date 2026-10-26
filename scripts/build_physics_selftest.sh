#!/bin/bash
# Build isolated validation only; does not patch translated units or deploy.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/generate_physics_fast.py
python3 scripts/patch_physics_fast.py
python3 scripts/prepare_physics_tests.py
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -v "$PWD:/src:ro" -v "$PWD/build/physics-stage:/out:rw" -w /src \
 "${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}" \
 aarch64-linux-gnu-gcc -std=gnu11 -O2 -mcpu=cortex-a53 -fno-strict-aliasing -fwrapv \
 -ffp-contract=off -DRECOMP_FP_STACK_EXACT=1 -static -Iruntime \
 -Ithird_party/pcrecomp/runtime/recomp32 tests/physics_fast.c runtime/physics_fast.c \
 build/physics-stage/full-native.c build/physics-stage/full-reference.c \
 build/physics-stage/precise-callees.c runtime/fp_stack_exact.c \
 -lm -o /out/promoted-full-selftest-exact.aarch64
sha256sum build/physics-stage/promoted-full-selftest-exact.aarch64
