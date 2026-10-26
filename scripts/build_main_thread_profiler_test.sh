#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/main-thread-profiler
sha256sum probe/main_thread_profiler.c tests/main_thread_profiler.c runtime/profiler.h > build/main-thread-profiler/INPUT-SHA256SUMS
image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
docker run --rm --network none --read-only --tmpfs /tmp \
  --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" \
  -v "$PWD/build/main-thread-profiler:/out:rw" "$image" \
  aarch64-linux-gnu-gcc -std=gnu11 -O2 -g -Wall -Wextra -Werror -mcpu=cortex-a53 \
  -I/src/runtime /src/probe/main_thread_profiler.c /src/tests/main_thread_profiler.c \
  -pthread -lrt -o /out/test.aarch64
sha256sum -c build/main-thread-profiler/INPUT-SHA256SUMS
sha256sum build/main-thread-profiler/test.aarch64 > build/main-thread-profiler/SHA256SUMS
readelf -h build/main-thread-profiler/test.aarch64 > build/main-thread-profiler/elf-header.txt
