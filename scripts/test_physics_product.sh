#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/stage_physics_product.py
# Match the frozen0097 compilation, including its contraction-off pragma.
common=(-std=gnu11 -O2 -fno-strict-aliasing -fwrapv -ffp-contract=off -DRECOMP_FP_STACK_EXACT=1 -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 -Ibuild/physics-product)
sources=(build/physics-product/full-test.c runtime/physics_fast.c build/physics-product/physics_product.c build/physics-product/full-native.c build/physics-product/full-reference.c build/physics-product/precise-callees.c runtime/fp_stack_exact.c)
kernel=(tests/physics_product.c build/physics-product/physics_product.c build/physics-product/kernel.c build/physics-product/reference.c runtime/fp_stack_exact.c)
cc "${common[@]}" "${kernel[@]}" -lm -o build/physics-product/kernel-selftest
env -u DS_FAST_PHYSICS_PRODUCT build/physics-product/kernel-selftest --disabled
DS_FAST_PHYSICS_PRODUCT=0 build/physics-product/kernel-selftest --disabled
DS_FAST_PHYSICS_PRODUCT=1 build/physics-product/kernel-selftest
cc "${common[@]}" "${sources[@]}" -lm -o build/physics-product/full-selftest
DS_FAST_MATH=1 DS_FAST_PHYSICS_PRODUCT=1 build/physics-product/full-selftest
DS_FAST_MATH=1 DS_FAST_PHYSICS_PRODUCT=0 build/physics-product/full-selftest
if [[ ${1:-} == --cross ]]; then
 image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
  -v "$PWD:/src:ro" -v "$PWD/build/physics-product:/out:rw" --workdir /src "$image" \
  aarch64-linux-gnu-gcc "${common[@]}" -mcpu=cortex-a53 -static "${sources[@]}" -lm -o /out/full-selftest.aarch64
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
  -v "$PWD:/src:ro" -v "$PWD/build/physics-product:/out:rw" --workdir /src "$image" \
  aarch64-linux-gnu-gcc "${common[@]}" -mcpu=cortex-a53 -static "${kernel[@]}" -lm -o /out/kernel-selftest.aarch64
fi
