#!/usr/bin/env bash
# Host differential test for RECOMP_FP_WINDOW; --cross also builds a static
# AArch64 binary (build/fp-window-tests/fp-window-selftest.aarch64) to run on the device.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/fp-window-tests
"${CC:-cc}" -std=gnu11 -O2 -fno-strict-aliasing -fwrapv -Wall -Wextra -Werror \
    -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 tests/fp_window.c \
    -lm -o build/fp-window-tests/fp-window-selftest
build/fp-window-tests/fp-window-selftest
if [[ ${1:-} == --cross ]]; then
    image=${CROSS_IMAGE:-reconflict-cross:bookworm}
    docker run --rm --network none --read-only --tmpfs /tmp \
        --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" \
        -v "$PWD/build/fp-window-tests:/out:rw" --workdir /src "$image" \
        aarch64-linux-gnu-gcc -std=gnu11 -O2 -mcpu=cortex-a53 -fno-strict-aliasing -fwrapv -Wall -Wextra -Werror \
            -static -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 tests/fp_window.c \
            -lm -o /out/fp-window-selftest.aarch64
elif [[ -n ${1:-} ]]; then
    echo 'Usage: test_fp_window.sh [--cross]' >&2
    exit 1
fi
