#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/stage_rotation_scalar.py
flags=(-O2 -fno-strict-aliasing -fwrapv -ffp-contract=off -frounding-math -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 -Ibuild/rotation-scalar)
cc "${flags[@]}" tests/rotation_scalar.c build/rotation-scalar/rotation_scalar.c build/rotation-scalar/reference.c -lm -o build/rotation-scalar/selftest
env -u DS_FAST_ROTATION build/rotation-scalar/selftest --disabled
DS_FAST_ROTATION=0 build/rotation-scalar/selftest --disabled
DS_FAST_ROTATION=1 build/rotation-scalar/selftest
if [[ ${1:-} == --cross ]]; then
 image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" -v "$PWD/build/rotation-scalar:/out:rw" --workdir /src "$image" bash -eu -c '
   common="-std=gnu11 -O2 -fno-strict-aliasing -fwrapv -mcpu=cortex-a53 -DRECOMP_FP_STACK_EXACT=1 -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 -Ibuild/rotation-scalar"
   # Match the frozen generated unit: reference uses the original contraction
   # and rounding defaults; only the optimized helper adds the strict flags.
   aarch64-linux-gnu-gcc $common -c build/rotation-scalar/reference.c -o /out/reference.aarch64.o
   aarch64-linux-gnu-gcc $common -ffp-contract=off -frounding-math tests/rotation_scalar.c build/rotation-scalar/rotation_scalar.c runtime/fp_stack_exact.c /out/reference.aarch64.o -lm -o /out/rotation-scalar-selftest.aarch64
 '

fi
