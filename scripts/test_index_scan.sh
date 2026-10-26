#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/stage_index_scan.py
cc -std=gnu11 -O3 -Wall -Wextra -Iprobe tests/index_scan.c -o build/index-scan/selftest
build/index-scan/selftest
if [[ ${1:-} == --cross ]];then
 image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -v "$PWD:/src:ro" -v "$PWD/build/index-scan:/out:rw" --workdir /src "$image" \
 aarch64-linux-gnu-gcc -std=gnu11 -O3 -mcpu=cortex-a53 -Wall -Wextra -static -Iprobe tests/index_scan.c -o /out/selftest.aarch64
fi
