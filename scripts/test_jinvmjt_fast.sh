#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/prepare_jinvmjt_tests.py
flags=(-O2 -fno-strict-aliasing -fwrapv -ffp-contract=off -Iruntime -Ithird_party/pcrecomp/runtime/recomp32)
cc "${flags[@]}" tests/jinvmjt_fast.c runtime/jinvmjt_fast.c build/jinvmjt-tests/blocks-reference.c -lm -o build/jinvmjt-tests/block-test
env -u DS_FAST_MATH build/jinvmjt-tests/block-test --disabled
DS_FAST_MATH=0 build/jinvmjt-tests/block-test --disabled
DS_FAST_MATH=1 build/jinvmjt-tests/block-test
cc "${flags[@]}" tests/jinvmjt_full.c runtime/jinvmjt_fast.c build/jinvmjt-tests/full-reference.c build/jinvmjt-tests/full-native.c build/jinvmjt-tests/precise-callees.c -lm -o build/jinvmjt-tests/full-test
DS_FAST_MATH=1 build/jinvmjt-tests/full-test
DS_FAST_MATH=0 build/jinvmjt-tests/full-test
if [[ ${1:-} == --cross ]]; then
 image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
 for mode in block full; do
  test_source=tests/jinvmjt_fast.c
  [[ $mode == full ]] && test_source=tests/jinvmjt_full.c
  extra=(build/jinvmjt-tests/blocks-reference.c)
  [[ $mode == full ]] && extra=(build/jinvmjt-tests/full-reference.c build/jinvmjt-tests/full-native.c build/jinvmjt-tests/precise-callees.c)
  docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" -v "$PWD/build/jinvmjt-tests:/out:rw" --workdir /src "$image" aarch64-linux-gnu-gcc "${flags[@]}" -mcpu=cortex-a53 -DRECOMP_FP_STACK_EXACT=1 $test_source runtime/jinvmjt_fast.c runtime/fp_stack_exact.c "${extra[@]}" -lm -o /out/jinvmjt-$mode-selftest.aarch64
 done
fi
