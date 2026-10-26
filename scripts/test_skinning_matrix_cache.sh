#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=${DS_SKINNING_TEST_OUTPUT:-build/skinning-cache-tests}
source=${DS_SKINNING_TEST_SOURCE:-probe/skinning_matrix_cache.c}
[[ "$out" == build/* && "$out" != *..* && -f "$source" ]] || { echo 'Invalid test source/output' >&2; exit 1; }
mkdir -p "$out"
# Reuse the independently extracted original reference, checking its identity.
printf '%s\n' 'd571490d0ffe01f2be010de030c3b0ec62073e881bdd99cc94f848883b9b0ec7  build/skinning-lanes-stage/reference.c' | sha256sum -c -
sha256sum "$source" tests/skinning_matrix_cache.c runtime/skinning_fast.c \
  build/skinning-lanes-stage/reference.c build/skinning-lanes-stage/baseline.c > "$out/INPUT-SHA256SUMS"
flags=(-std=gnu11 -O2 -ffp-contract=off -Iruntime -Ithird_party/pcrecomp/runtime/recomp32)
cc "${flags[@]}" -Dds_skinning_fast_529520=ds_skinning_cache_529520 -c "$source" -o "$out/cache-host.o"
cc "${flags[@]}" -c runtime/skinning_fast.c -o "$out/baseline-host.o"
cc "${flags[@]}" -fno-strict-aliasing -fwrapv tests/skinning_matrix_cache.c \
  build/skinning-lanes-stage/reference.c build/skinning-lanes-stage/baseline.c \
  "$out/cache-host.o" "$out/baseline-host.o" -lm -o "$out/test-host"
DS_FAST_MATH=0 "$out/test-host" --disabled
DS_FAST_MATH=1 "$out/test-host" | tee "$out/host-test.log"
if [[ ${1:-} == --cross ]]; then
 image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
  -e DS_SKINNING_TEST_SOURCE="$source" \
  -v "$PWD:/src:ro" -v "$PWD/$out:/out:rw" --workdir /src "$image" bash -eu -o pipefail -c '
   flags="-std=gnu11 -O2 -mcpu=cortex-a53 -ffp-contract=off -Iruntime -Ithird_party/pcrecomp/runtime/recomp32"
   aarch64-linux-gnu-gcc $flags -Dds_skinning_fast_529520=ds_skinning_cache_529520 -c "$DS_SKINNING_TEST_SOURCE" -o /out/cache.o
   aarch64-linux-gnu-gcc $flags -c runtime/skinning_fast.c -o /out/baseline.o
   aarch64-linux-gnu-gcc $flags -fno-strict-aliasing -fwrapv -DRECOMP_FP_STACK_EXACT=1 \
    tests/skinning_matrix_cache.c build/skinning-lanes-stage/reference.c \
    build/skinning-lanes-stage/baseline.c runtime/fp_stack_exact.c /out/cache.o /out/baseline.o \
    -lm -o /out/test.aarch64
  '
 sha256sum "$out/test.aarch64" > "$out/SHA256SUMS"
fi
sha256sum -c "$out/INPUT-SHA256SUMS"
