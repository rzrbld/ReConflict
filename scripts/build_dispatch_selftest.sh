#!/bin/bash
# Isolated validation build only; no game build, patch mutation or deployment.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/patch_dispatch_cache.py
python3 scripts/prepare_dispatch_tests.py
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -v "$PWD:/src:ro" -v "$PWD/build/lookup-stage:/out:rw" -w /src \
 "${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}" \
 bash -eu -o pipefail -c '
 for size in 256 1024;do
  for kind in selftest benchmark;do
   source=tests/dispatch_cache.c
   [[ "$kind" == benchmark ]] && source=tests/dispatch_cache_benchmark.c
   aarch64-linux-gnu-gcc -std=gnu11 -O2 -mcpu=cortex-a53 -DDS_MAIN_LOOKUP_CACHE=1 -DDS_DISPATCH_CACHE_SIZE=$size -DDS_LOOKUP_CACHE_TEST -ffunction-sections -fdata-sections -static \
    -Iruntime -Ibuild/lookup-stage -Ithird_party/pcrecomp/runtime/win32hle -Ithird_party/pcrecomp/runtime/recomp32 \
    "$source" build/lookup-stage/promoted-host_lite.c runtime/dispatch_cache.c -Wl,--gc-sections -lm -pthread -o /out/promoted-$kind-$size.aarch64
  done
 done
 aarch64-linux-gnu-gcc -std=gnu11 -O2 -mcpu=cortex-a53 -DDS_MAIN_LOOKUP_CACHE=0 -DDS_LOOKUP_CACHE_TEST -ffunction-sections -fdata-sections -static \
  -Iruntime -Ibuild/lookup-stage -Ithird_party/pcrecomp/runtime/win32hle -Ithird_party/pcrecomp/runtime/recomp32 \
  tests/dispatch_cache_benchmark.c build/lookup-stage/baseline-host_lite.c runtime/dispatch_cache.c -Wl,--gc-sections -lm -pthread -o /out/promoted-benchmark-baseline.aarch64
 '
sha256sum build/lookup-stage/promoted-*.aarch64
