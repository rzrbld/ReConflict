#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/matrix-inverse-tests
python3 scripts/generate_matrix_inverse_fast.py
cc -O2 -fno-strict-aliasing -fwrapv -ffp-contract=off -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 tests/matrix_inverse_fast.c runtime/matrix_inverse_fast.c build/matrix-inverse-tests/reference.c build/matrix-inverse-tests/finite-reference.c -lm -o build/matrix-inverse-tests/selftest
env -u DS_FAST_MATH build/matrix-inverse-tests/selftest --disabled
DS_FAST_MATH=0 build/matrix-inverse-tests/selftest --disabled
DS_FAST_MATH=1 build/matrix-inverse-tests/selftest
if [[ ${1:-} == --cross ]]; then
 image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" -v "${PWD}/build/matrix-inverse-tests:/out:rw" --workdir /src "$image" aarch64-linux-gnu-gcc \
 -O2 -mcpu=cortex-a53 -fno-strict-aliasing -fwrapv -ffp-contract=off -DRECOMP_FP_STACK_EXACT=1 -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 \
 tests/matrix_inverse_fast.c runtime/matrix_inverse_fast.c runtime/fp_stack_exact.c build/matrix-inverse-tests/reference.c build/matrix-inverse-tests/finite-reference.c -lm -o /out/matrix-inverse-fast-selftest.aarch64
fi
