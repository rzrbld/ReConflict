#!/bin/bash
# Synthetic address locality only: never presented as gameplay FPS.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/prepare_dispatch_tests.py
for size in 256 1024;do
 "${CC:-cc}" -std=gnu11 -O2 -DDS_MAIN_LOOKUP_CACHE=1 -DDS_DISPATCH_CACHE_SIZE=$size -DDS_LOOKUP_CACHE_TEST -ffunction-sections -fdata-sections \
  -Iruntime -Ibuild/lookup-stage -Ithird_party/pcrecomp/runtime/win32hle -Ithird_party/pcrecomp/runtime/recomp32 \
  tests/dispatch_cache_benchmark.c build/lookup-stage/promoted-host_lite.c runtime/dispatch_cache.c -Wl,--gc-sections -lm -pthread -o build/lookup-stage/promoted-benchmark-$size
 DS_DISPATCH_CACHE=0 build/lookup-stage/promoted-benchmark-$size
 DS_DISPATCH_CACHE=1 build/lookup-stage/promoted-benchmark-$size
done
"${CC:-cc}" -std=gnu11 -O2 -DDS_MAIN_LOOKUP_CACHE=0 -DDS_LOOKUP_CACHE_TEST -ffunction-sections -fdata-sections \
 -Iruntime -Ibuild/lookup-stage -Ithird_party/pcrecomp/runtime/win32hle -Ithird_party/pcrecomp/runtime/recomp32 \
 tests/dispatch_cache_benchmark.c build/lookup-stage/baseline-host_lite.c runtime/dispatch_cache.c -Wl,--gc-sections -lm -pthread -o build/lookup-stage/promoted-benchmark-baseline
DS_DISPATCH_CACHE=0 build/lookup-stage/promoted-benchmark-baseline
