#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/stage_dense_profiler.py
out=build/dense-profiler
for test in main_thread_profiler dense_scene_profiler;do
 cc -std=gnu11 -O2 -g -Wall -Wextra -Werror -Iruntime "$out/profiler.c" "tests/$test.c" -pthread -lrt -o "$out/$test-host"
 "$out/$test-host" > "$out/$test-host.log" 2>&1
 tail -1 "$out/$test-host.log"
done
if [[ ${1:-} == --cross ]];then
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -v "$PWD:/src:ro" -v "$PWD/$out:/out:rw" --workdir /src \
 sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49 bash -eu -c '
 for test in main_thread_profiler dense_scene_profiler;do
  aarch64-linux-gnu-gcc -std=gnu11 -O2 -g -mcpu=cortex-a53 -Wall -Wextra -Werror -Iruntime /out/profiler.c tests/$test.c -pthread -lrt -o /out/$test.aarch64
 done'
 sha256sum "$out"/*.aarch64 > "$out/TEST-SHA256SUMS"
fi
