#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/generate_culling_prefix_tests.py
for mode in prefix full; do
 cc -O2 -fno-strict-aliasing -fwrapv -ffp-contract=off -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 tests/culling_prefix.c runtime/culling_fast.c build/culling-prefix-tests/$mode.c -lm -o build/culling-prefix-tests/$mode-test
 env -u DS_FAST_CULLING build/culling-prefix-tests/$mode-test --disabled
 DS_FAST_CULLING=0 build/culling-prefix-tests/$mode-test --disabled
 build/culling-prefix-tests/$mode-test > build/culling-prefix-tests/$mode-host-test.log
 done
if [[ ${1:-} == --cross ]]; then
 image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
 for mode in prefix full; do
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" -v "$PWD/build/culling-prefix-tests:/out:rw" --workdir /src "$image" aarch64-linux-gnu-gcc -O2 -mcpu=cortex-a53 -fno-strict-aliasing -fwrapv -ffp-contract=off -DRECOMP_FP_STACK_EXACT=1 -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 tests/culling_prefix.c runtime/culling_fast.c runtime/fp_stack_exact.c build/culling-prefix-tests/$mode.c -lm -o /out/culling-$mode-selftest.aarch64
 done
fi
