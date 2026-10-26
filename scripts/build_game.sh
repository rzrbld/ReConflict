#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/patch_dispatch_cache.py --apply
python3 scripts/generate_modules.py
PYTHONDONTWRITEBYTECODE=1 python3 scripts/recover_targets.py
mkdir -p build/game/obj
image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
docker run --rm --network none --read-only --tmpfs /tmp \
  --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" -v "$PWD/build/game:/out:rw" --workdir /src \
  "$image" bash -eu -o pipefail -c '
    flags="-std=gnu11 -O1 -g -DDS_HEAP_START=0x40000000u -I/src/runtime -I/src/probe -I/src/third_party/pcrecomp/runtime/recomp32 -I/src/third_party/pcrecomp/runtime/win32hle -I/src/build/sdk-include -I/src/build/sdk-include/SDL2 -I/usr/include/SDL2"
    for src in /src/third_party/pcrecomp/runtime/win32hle/*.c; do
      [[ "$src" == *_selftest.c ]] && continue
      name=$(basename "$src" .c)
      aarch64-linux-gnu-gcc $flags -c "$src" -o "/out/obj/hle_$name.o"
    done
    for src in /src/runtime/*.c /src/build/game/main_dispatch.c /src/build/game/modules.c /src/build/game/recovered*.c /src/probe/desert_pad.c; do
      [[ -f "$src" ]] || continue
      [[ "$src" == *_selftest.c ]] && continue
      name=$(basename "$src" .c)
      optimize=""
      # The device profile identified per-vertex matrix work as a CPU hotspot.
      # Enable normal compiler vectorization; preserve IEEE FP semantics.
      [[ "$name" == d3d8_renderer ]] && optimize="-O3 -mcpu=cortex-a53"
      [[ "$name" == d3d8 ]] && optimize="-O2 -fno-strict-aliasing -fwrapv -ffp-contract=off -mcpu=cortex-a53"
      [[ "$name" == math_fast || "$name" == matrix_multiply_fast || "$name" == skinning_fast || "$name" == culling_fast || "$name" == audio_decode_fast ]] && optimize="-O2 -ffp-contract=off -mcpu=cortex-a53"
      [[ "$name" == jinvmjt_fast || "$name" == physics_fast || "$name" == quaternion_fast || "$name" == matrix_inverse_fast || "$name" == geometry_fast || "$name" == culling_fast ]] && optimize="-O2 -fno-strict-aliasing -fwrapv -ffp-contract=off -mcpu=cortex-a53"
      [[ "$name" == fp_stack_exact ]] && optimize="-O2 -mcpu=cortex-a53"
      aarch64-linux-gnu-gcc $flags $optimize -c "$src" -o "/out/obj/ds_$name.o"
    done
    archives=()
    for module in /src/build/translated/*; do [[ -d "$module" ]] && archives+=("$module/lifted.aarch64.a"); done
    aarch64-linux-gnu-gcc -g -rdynamic -o /out/desert-storm.aarch64 /out/obj/*.o \
      -Wl,--start-group "${archives[@]}" -Wl,--end-group \
      -L/src/build/target-libs -Wl,--allow-shlib-undefined -lSDL2 -lSDL2_ttf -lGLESv2 -lm -lpthread -ldl
  '
readelf -h build/game/desert-storm.aarch64 > build/game/elf-header.txt
readelf -d build/game/desert-storm.aarch64 > build/game/dynamic.txt
sha256sum build/game/desert-storm.aarch64 > build/game/SHA256SUMS
