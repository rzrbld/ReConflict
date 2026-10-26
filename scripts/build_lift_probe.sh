#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
docker run --rm --network none --read-only --tmpfs /tmp \
  --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" \
  -v "$PWD/build/lift-probe:/out:rw" --workdir /src "$image" sh -eu -c '
    for src in /src/build/lift-probe/*.c; do
      name=$(basename "$src" .c)
      aarch64-linux-gnu-gcc -std=gnu11 -O2 -Werror=implicit-function-declaration \
        -I/src/third_party/pcrecomp/runtime/recomp32 \
        -c "$src" -o "/out/$name.aarch64.o"
    done
  '
for object in build/lift-probe/*.o; do
  readelf -h "$object"
done
