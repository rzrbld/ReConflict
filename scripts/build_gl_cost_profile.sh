#!/usr/bin/env bash
# Native diagnostic relink only. Uses current frozen objects/archive inputs;
# never overwrites normal build/game objects/executable or deploys anything.
set -euo pipefail
cd "$(dirname "$0")/.."
[[ -f build/game/obj/ds_d3d8.o ]] || { echo 'Build native runtime first' >&2; exit 1; }
nm -u build/game/obj/ds_d3d8.o | rg -q '\bds_gl_cost_present$' || {
  echo 'Current facade object has no ds_gl_cost_present hook; rebuild the facade before this diagnostic relink' >&2
  exit 1
}
mkdir -p build/gl-cost-profile
python3 scripts/gl_cost_flags.py > build/gl-cost-profile/wrapflags.txt
sha256sum build/game/obj/*.o build/translated/*/lifted.aarch64.a > build/gl-cost-profile/INPUT-SHA256SUMS
image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
docker run --rm --network none --read-only --tmpfs /tmp \
  --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" \
  -v "$PWD/build/gl-cost-profile:/profile:rw" --workdir /src \
  "$image" bash -eu -o pipefail -c '
    aarch64-linux-gnu-gcc -std=c11 -O2 -g -mcpu=cortex-a53 \
      -I/src/build/sdk-include -I/src/probe -Wall -Wextra -Werror \
      -c /src/probe/gl_cost_profile.c -o /profile/gl_cost_profile.o
    archives=()
    for module in /src/build/translated/*; do [[ -d "$module" ]] && archives+=("$module/lifted.aarch64.a"); done
    aarch64-linux-gnu-gcc -g -rdynamic -o /profile/game-gl-cost.aarch64 \
      /src/build/game/obj/*.o /profile/gl_cost_profile.o @/profile/wrapflags.txt \
      -Wl,--start-group "${archives[@]}" -Wl,--end-group \
      -L/src/build/target-libs -Wl,--allow-shlib-undefined \
      -lSDL2 -lSDL2_ttf -lGLESv2 -lm -lpthread -ldl
  '
readelf -h build/gl-cost-profile/game-gl-cost.aarch64 > build/gl-cost-profile/elf-header.txt
sha256sum build/gl-cost-profile/game-gl-cost.aarch64 > build/gl-cost-profile/SHA256SUMS
