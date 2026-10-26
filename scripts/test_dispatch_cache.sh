#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/patch_dispatch_cache.py
python3 scripts/prepare_dispatch_tests.py
for size in 256 1024;do
 "${CC:-cc}" -std=gnu11 -O2 -DDS_MAIN_LOOKUP_CACHE=1 -DDS_DISPATCH_CACHE_SIZE=$size -DDS_LOOKUP_CACHE_TEST -ffunction-sections -fdata-sections \
  -Iruntime -Ithird_party/pcrecomp/runtime/win32hle -Ithird_party/pcrecomp/runtime/recomp32 \
  tests/dispatch_cache.c build/lookup-stage/promoted-host_lite.c runtime/dispatch_cache.c -Wl,--gc-sections -lm -pthread -o build/lookup-stage/promoted-selftest-$size
 env -u DS_DISPATCH_CACHE build/lookup-stage/promoted-selftest-$size
 DS_DISPATCH_CACHE=0 build/lookup-stage/promoted-selftest-$size
 DS_DISPATCH_CACHE=1 build/lookup-stage/promoted-selftest-$size
done
# The generic HLE host must also work when the optional runtime is not linked.
"${CC:-cc}" -std=gnu11 -O2 -ffunction-sections -fdata-sections \
 -Ithird_party/pcrecomp/runtime/win32hle -Ithird_party/pcrecomp/runtime/recomp32 \
 tests/strict_dispatch.c build/lookup-stage/promoted-host_lite.c \
 -Wl,--gc-sections -lm -pthread -o build/lookup-stage/strict-without-cache-helper
build/lookup-stage/strict-without-cache-helper
