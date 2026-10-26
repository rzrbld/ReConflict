#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/fp-stack-tests
"${CC:-cc}" -std=gnu11 -O2 -fno-strict-aliasing -fwrapv -Wall -Wextra -Werror \
    -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 tests/fp_stack_exact.c runtime/fp_stack_exact.c \
    -lm -o build/fp-stack-tests/fp-stack-exact-selftest
build/fp-stack-tests/fp-stack-exact-selftest
if [[ ${1:-} == --cross ]]; then
    image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
    docker run --rm --network none --read-only --tmpfs /tmp \
        --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" \
        -v "$PWD/build/fp-stack-tests:/out:rw" --workdir /src "$image" bash -eu -c '
        aarch64-linux-gnu-gcc -std=gnu11 -O2 -mcpu=cortex-a53 -fno-strict-aliasing -fwrapv -Wall -Wextra -Werror \
            -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 tests/fp_stack_exact.c runtime/fp_stack_exact.c \
            -lm -o /out/fp-stack-exact-selftest.aarch64
        aarch64-linux-gnu-gcc -std=gnu11 -O2 -mcpu=cortex-a53 -fno-strict-aliasing -fwrapv -Wall -Wextra -Werror \
            -DRECOMP_FP_STACK_EXACT=1 -DDS_FP_TEST_MACROS=1 \
            -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 tests/fp_stack_exact.c runtime/fp_stack_exact.c \
            -lm -o /out/fp-stack-exact-macro-selftest.aarch64
        aarch64-linux-gnu-objdump -d /out/fp-stack-exact-selftest.aarch64 > /out/disassembly.txt'
elif [[ -n ${1:-} ]]; then
    echo 'Usage: test_fp_stack_exact.sh [--cross]' >&2
    exit 1
fi
