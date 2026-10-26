#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/generate_quaternion_fast.py
cc -O2 -fno-strict-aliasing -fwrapv -ffp-contract=off -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 tests/quaternion_fast.c runtime/quaternion_fast.c build/quaternion-tests/reference.c -lm -o build/quaternion-tests/selftest
env -u DS_FAST_MATH build/quaternion-tests/selftest --disabled
DS_FAST_MATH=0 build/quaternion-tests/selftest --disabled
DS_FAST_MATH=1 build/quaternion-tests/selftest
if [[ ${1:-} == --cross ]]; then
 image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" -v "$PWD/build/quaternion-tests:/out:rw" --workdir /src "$image" aarch64-linux-gnu-gcc -O2 -mcpu=cortex-a53 -fno-strict-aliasing -fwrapv -ffp-contract=off -DRECOMP_FP_STACK_EXACT=1 -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 tests/quaternion_fast.c runtime/quaternion_fast.c runtime/fp_stack_exact.c build/quaternion-tests/reference.c -lm -o /out/quaternion-fast-selftest.aarch64
fi
