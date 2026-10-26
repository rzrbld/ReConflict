#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/stage_present_timing.py
out=build/present-timing
cc -std=gnu11 -O2 -g -Wall -Wextra -Werror -Iprobe tests/present_timing.c probe/present_timing.c -pthread -o "$out/selftest"
"$out/selftest" --disabled
"$out/selftest"
if [[ ${1:-} == --cross ]];then
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" -v "$PWD/$out:/out:rw" --workdir /src sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49 aarch64-linux-gnu-gcc -std=gnu11 -O2 -g -mcpu=cortex-a53 -Wall -Wextra -Werror -Iprobe tests/present_timing.c probe/present_timing.c -pthread -o /out/selftest.aarch64
fi
