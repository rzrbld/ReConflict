#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/main-thread-profiler
cc -std=gnu11 -O2 -g -Wall -Wextra -Werror -Iruntime \
  probe/main_thread_profiler.c tests/main_thread_profiler.c -pthread -lrt \
  -o build/main-thread-profiler/test-host
build/main-thread-profiler/test-host 2>&1 | tee build/main-thread-profiler/host-test.log
