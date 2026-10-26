#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/frame-limit-tests
read -r -a cflags <<< "$(pkg-config --cflags sdl2)"
cc -std=gnu11 -O2 -Wall -Wextra -Werror -Iruntime "${cflags[@]}" tests/frame_limiter.c -o build/frame-limit-tests/test-host
for cap in 30 60 0; do build/frame-limit-tests/test-host "$cap"; done
if [[ ${1:-} == --cross ]];then
 image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
  -v "$PWD:/src:ro" -v "$PWD/build/frame-limit-tests:/out:rw" --workdir /src "$image" \
  bash -eu -c 'aarch64-linux-gnu-gcc -std=gnu11 -O2 -Wall -Wextra -Werror -Iruntime -Ibuild/sdk-include -Ibuild/sdk-include/SDL2 -I/usr/include/SDL2 tests/frame_limiter.c -o /out/test.aarch64'
fi
