#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/stage_fp_inline.py
common=(-std=gnu11 -O2 -fno-strict-aliasing -fwrapv -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 -Ibuild/fp-inline)
cc "${common[@]}" tests/fp_inline.c build/fp-inline/reference.c build/fp-inline/candidate.c runtime/fp_stack_exact.c -lm -o build/fp-inline/selftest
build/fp-inline/selftest
cc "${common[@]}" -DDS_FP_TEST_INLINE=1 -DDS_FP_TEST_MACROS=1 tests/fp_stack_exact.c runtime/fp_stack_exact.c -lm -o build/fp-inline/raw-selftest
build/fp-inline/raw-selftest
if [[ ${1:-} == --cross ]]; then
 image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" -v "$PWD/build/fp-inline:/out:rw" --workdir /src "$image" bash -eu -c '
  common="-std=gnu11 -O2 -fno-strict-aliasing -fwrapv -mcpu=cortex-a53 -DRECOMP_FP_STACK_EXACT=1 -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 -Ibuild/fp-inline"
  aarch64-linux-gnu-gcc $common -c build/fp-inline/reference.c -o /out/reference.aarch64.o
  aarch64-linux-gnu-gcc $common -c build/fp-inline/candidate.c -o /out/candidate.aarch64.o
  aarch64-linux-gnu-gcc $common tests/fp_inline.c runtime/fp_stack_exact.c /out/reference.aarch64.o /out/candidate.aarch64.o -lm -o /out/selftest.aarch64
  aarch64-linux-gnu-gcc $common -DDS_FP_TEST_INLINE=1 -DDS_FP_TEST_MACROS=1 tests/fp_stack_exact.c runtime/fp_stack_exact.c -lm -o /out/raw-selftest.aarch64
  aarch64-linux-gnu-objdump -dr /out/candidate.aarch64.o > /out/candidate-disassembly.txt
 '
fi
