#!/usr/bin/env bash
# Isolated translated-code LTO experiment. Never builds/deploys the game.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/stage_lto.py "$@"
image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
jobs=${LTO_JOBS:-10}
[[ "$jobs" =~ ^([1-9]|1[012])$ ]] || { echo 'LTO_JOBS must be1..12' >&2; exit 1; }
docker image inspect --format '{{.Id}}' "$image" > build/lto-stage/output/docker-image.txt
docker run --rm --network none --read-only --tmpfs /tmp --memory=64g --memory-swap=64g \
  --user "$(id -u):$(id -g)" -v "$PWD/build/lto-stage/src:/input:ro" \
  -v "$PWD/build/lto-stage/output:/out:rw" "$image" bash -eu -o pipefail -c '
    aarch64-linux-gnu-gcc --version > /out/compiler.current.txt
    [[ "$(aarch64-linux-gnu-gcc -dumpversion)" == 12 ]] || { echo "Expected pinned GCC12" >&2; exit 1; }
    if [[ -f /out/compiler.txt ]]; then cmp /out/compiler.txt /out/compiler.current.txt; else mv /out/compiler.current.txt /out/compiler.txt; fi
  '
python3 scripts/stage_lto.py --jobs
started=$(date +%s)
run_id=$(date +%s%N)
status=0
docker run --rm --network none --read-only --tmpfs /tmp --memory=64g --memory-swap=64g \
  --user "$(id -u):$(id -g)" -e LTO_JOBS="$jobs" \
  -e LTO_RUN_ID="$run_id" \
  -v "$PWD/build/lto-stage/src:/input:ro" -v "$PWD/build/lto-stage/output:/out:rw" \
  -v "$PWD/scripts:/scripts:ro" "$image" bash -eu -o pipefail -c '
    export LC_ALL=C
    xargs -r -P "$LTO_JOBS" -I {} bash /scripts/lto_compile_worker.sh {} < /out/compile-jobs.txt
    for module in /input/translated/*; do
      [[ -d "$module" ]] || continue
      name=$(basename "$module")
      aarch64-linux-gnu-gcc-ar rcsD "/out/$name/lifted.aarch64.a.new" "/out/$name/"*.aarch64.o
      mv "/out/$name/lifted.aarch64.a.new" "/out/$name/lifted.aarch64.a"
    done
    aarch64-linux-gnu-gcc-ar rcsD /out/support.aarch64.a.new /out/support/*.aarch64.o
    mv /out/support.aarch64.a.new /out/support.aarch64.a
  ' || status=$?
finished=$(date +%s)
printf 'parallel_workers=%s memory_limit_gib=64 wall_seconds=%s start_unix=%s end_unix=%s status=%s\n' "$jobs" "$((finished-started))" "$started" "$finished" "$status" >> build/lto-stage/output/build-time.txt
python3 scripts/stage_lto.py --record-success --run-id "$run_id"
[[ "$status" == 0 ]] || exit "$status"
sha256sum build/lto-stage/output/*/*.aarch64.a build/lto-stage/output/support.aarch64.a > build/lto-stage/output/ARCHIVE-SHA256SUMS
