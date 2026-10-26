#!/bin/bash
# Isolated ARM probe only; no game build, deployment or device access.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/render-thread-tests
python3 scripts/generate_render_proxy.py
python3 scripts/stage_render_worker_parity.py
python3 scripts/render_worker_test_manifest.py build/render-thread-tests/arm-inputs.json
image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -v "$PWD:/src:ro" -v "$PWD/build/render-thread-tests:/out:rw" -w /src "$image" bash -eu -o pipefail -c '
 flags="-std=gnu11 -O2 -Wall -Wextra -Iprobe -Ibuild/render-thread-stage -Ibuild/sdk-include -Ibuild/sdk-include/SDL2 -I/usr/include/SDL2"
 aarch64-linux-gnu-gcc $flags -c probe/render_thread_proxy.c -o /out/proxy-arm.o
 mapfile -t names < <(aarch64-linux-gnu-nm -g --defined-only /out/proxy-arm.o | awk '\''$3 ~ /^__wrap_[A-Za-z_][A-Za-z0-9_]*$/ {sub(/^__wrap_/,"",$3);print $3}'\'' | sort -u)
 test "${#names[@]}" -gt 0
 wrappers=();for name in "${names[@]}";do wrappers+=("-Wl,--wrap=$name");done
 aarch64-linux-gnu-gcc $flags -Werror probe/render_worker_regression.c /out/proxy-arm.o "${wrappers[@]}" \
  -Lbuild/target-libs -Wl,--allow-shlib-undefined -lSDL2 -lGLESv2 -pthread -lm -o /out/render-worker-regression.aarch64
 aarch64-linux-gnu-gcc -std=gnu11 -O2 -fno-strict-aliasing -ffp-contract=off \
  -Ibuild/render-thread-game/snapshot/runtime -Iprobe -Ibuild/render-thread-stage -Ibuild/sdk-include -Ibuild/sdk-include/SDL2 -I/usr/include/SDL2 \
  build/render-thread-tests/gpu_proxy_selftest.c build/render-thread-game/src/d3d8_renderer.c build/render-thread-tests/parity-lights.c /out/proxy-arm.o "${wrappers[@]}" \
  -Lbuild/target-libs -Wl,--allow-shlib-undefined -lSDL2 -lGLESv2 -pthread -lm -o /out/gpu-proxy-selftest.aarch64
 '
python3 scripts/render_worker_test_manifest.py build/render-thread-tests/arm-inputs.json --verify
sha256sum build/render-thread-tests/render-worker-regression.aarch64 build/render-thread-tests/gpu-proxy-selftest.aarch64
