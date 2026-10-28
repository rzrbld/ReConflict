#!/usr/bin/env bash
# Separate experimental relink; never changes the normal executable/objects.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/stage_render_thread_game.py
python3 scripts/generate_render_proxy.py
proxy_source=$(realpath "${DS_RT_PROXY_SOURCE:-probe/render_thread_proxy.c}")
[[ "$proxy_source" == "$PWD/"* && -f "$proxy_source" ]] || { echo 'Proxy source must be a repository file' >&2; exit 1; }
proxy_source=${proxy_source#"$PWD/"}
geometry_source=${DS_RT_GEOMETRY_SOURCE:-}
if [[ -n "$geometry_source" ]]; then
  geometry_source=$(realpath "$geometry_source")
  [[ "$geometry_source" == "$PWD/"* && -f "$geometry_source" ]] || { echo 'Geometry source must be a repository file' >&2; exit 1; }
  geometry_source=${geometry_source#"$PWD/"}
fi
profiler_source=${DS_RT_PROFILER_SOURCE:-}
if [[ -n "$profiler_source" ]]; then
  profiler_source=$(realpath "$profiler_source")
  [[ "$profiler_source" == "$PWD/"* && -f "$profiler_source" ]] || { echo 'Profiler source must be a repository file' >&2; exit 1; }
  profiler_source=${profiler_source#"$PWD/"}
fi
skinning_source=${DS_RT_SKINNING_SOURCE:-}
if [[ -n "$skinning_source" ]]; then
  skinning_source=$(realpath "$skinning_source")
  [[ "$skinning_source" == "$PWD/"* && -f "$skinning_source" ]] || { echo 'Skinning source must be a repository file' >&2; exit 1; }
  skinning_source=${skinning_source#"$PWD/"}
fi
renderer_source=$(realpath "${DS_RT_RENDERER_SOURCE:-build/render-thread-game/src/d3d8_renderer.c}")
[[ "$renderer_source" == "$PWD/"* && -f "$renderer_source" ]] || { echo 'Renderer source must be a repository file' >&2; exit 1; }
renderer_source=${renderer_source#"$PWD/"}
facade_source=$(realpath "${DS_RT_D3D8_SOURCE:-build/render-thread-game/src/d3d8.c}")
[[ "$facade_source" == "$PWD/"* && -f "$facade_source" ]] || { echo 'D3D8 source must be a repository file' >&2; exit 1; }
facade_source=${facade_source#"$PWD/"}
rotation_stage=${DS_RT_ROTATION_STAGE:-}
if [[ -n "$rotation_stage" ]]; then
  rotation_stage=$(realpath "$rotation_stage")
  [[ "$rotation_stage" == "$PWD/"* && -f "$rotation_stage/rotation_scalar.c" && -f "$rotation_stage/functions_0071.c" && -f "$rotation_stage/functions.h" ]] || { echo 'Invalid rotation stage' >&2; exit 1; }
  rotation_stage=${rotation_stage#"$PWD/"}
fi
length_stage=${DS_RT_LENGTH_STAGE:-}
if [[ -n "$length_stage" ]]; then
  length_stage=$(realpath "$length_stage")
  [[ "$length_stage" == "$PWD/"* && -f "$length_stage/length_scalar.c" && -f "$length_stage/functions_0065.c" && -f "$length_stage/functions.h" ]] || { echo 'Invalid length stage' >&2; exit 1; }
  length_stage=${length_stage#"$PWD/"}
fi
product_stage=${DS_RT_PHYSICS_PRODUCT_STAGE:-}
if [[ -n "$product_stage" ]]; then
  product_stage=$(realpath "$product_stage")
  [[ "$product_stage" == "$PWD/"* && -f "$product_stage/physics_product.c" && -f "$product_stage/physics_product.inc" && -f "$product_stage/functions_0097.c" && -f "$product_stage/functions.h" ]] || { echo 'Invalid physics product stage' >&2; exit 1; }
  product_stage=${product_stage#"$PWD/"}
fi
loading_stage=${DS_RT_LOADING_STAGE:-}
if [[ -n "$loading_stage" ]]; then
  loading_stage=$(realpath "$loading_stage")
  [[ "$loading_stage" == "$PWD/"* && -f "$loading_stage/functions_0003.c" && -f "$loading_stage/functions.h" ]] || { echo 'Invalid loading background stage' >&2; exit 1; }
  loading_stage=${loading_stage#"$PWD/"}
fi
label=${DS_RT_BUILD_LABEL:-}
[[ -z "$label" || "$label" =~ ^[a-zA-Z0-9][a-zA-Z0-9_-]*$ ]] || { echo 'Invalid build label' >&2; exit 1; }
output_subdir=.
[[ -z "$label" ]] || output_subdir="variants/$label"
output="build/render-thread-game/$output_subdir"
mkdir -p "$output/obj"
printf '%s\n' "$proxy_source" > "$output/PROXY-SOURCE.txt"
printf '%s\n' "$geometry_source" > "$output/GEOMETRY-SOURCE.txt"
printf '%s\n' "$profiler_source" > "$output/PROFILER-SOURCE.txt"
printf '%s\n' "$skinning_source" > "$output/SKINNING-SOURCE.txt"
printf '%s\n' "$renderer_source" > "$output/RENDERER-SOURCE.txt"
printf '%s\n' "$facade_source" > "$output/D3D8-SOURCE.txt"
printf '%s\n' "$rotation_stage" > "$output/ROTATION-STAGE.txt"
printf '%s\n' "$length_stage" > "$output/LENGTH-STAGE.txt"
printf '%s\n' "$product_stage" > "$output/PHYSICS-PRODUCT-STAGE.txt"
printf '%s\n' "$loading_stage" > "$output/LOADING-STAGE.txt"
proxy_directory=$(dirname "$proxy_source")
proxy_dependencies=()
for dependency in "$proxy_directory/render_thread_proxy.h" "$proxy_directory"/render_proxy_*.inc; do
  [[ ! -f "$dependency" ]] || proxy_dependencies+=("$dependency")
done
sha256sum "$proxy_source" "${proxy_dependencies[@]}" probe/render_thread_proxy.h \
  build/render-thread-stage/render_proxy_ops.inc \
  build/render-thread-stage/render_proxy_wrappers.inc \
  build/render-thread-game/src/*.c > "$output/EXPERIMENT-SHA256SUMS"
[[ -z "$geometry_source" ]] || sha256sum "$geometry_source" >> "$output/EXPERIMENT-SHA256SUMS"
[[ -z "$profiler_source" ]] || sha256sum "$profiler_source" >> "$output/EXPERIMENT-SHA256SUMS"
[[ -z "$skinning_source" ]] || sha256sum "$skinning_source" >> "$output/EXPERIMENT-SHA256SUMS"
sha256sum "$renderer_source" >> "$output/EXPERIMENT-SHA256SUMS"
renderer_directory=$(dirname "$renderer_source")
if [[ -f "$renderer_directory/index_scan.h" ]]; then
  sha256sum "$renderer_directory/index_scan.h" "$renderer_directory/manifest.json" >> "$output/EXPERIMENT-SHA256SUMS"
fi
sha256sum "$facade_source" >> "$output/EXPERIMENT-SHA256SUMS"
facade_directory=$(dirname "$facade_source")
if [[ -f "$facade_directory/present_timing.c" ]]; then
  sha256sum "$facade_directory/present_timing.c" "$facade_directory/present_timing.h" >> "$output/EXPERIMENT-SHA256SUMS"
fi
if [[ -f "$facade_directory/frame_limiter.h" ]]; then
  sha256sum "$facade_directory/frame_limiter.h" >> "$output/EXPERIMENT-SHA256SUMS"
fi
[[ -z "$rotation_stage" ]] || sha256sum "$rotation_stage"/*.c "$rotation_stage"/*.h "$rotation_stage/manifest.json" >> "$output/EXPERIMENT-SHA256SUMS"
[[ -z "$length_stage" ]] || sha256sum "$length_stage"/*.c "$length_stage"/*.h "$length_stage/manifest.json" >> "$output/EXPERIMENT-SHA256SUMS"
[[ -z "$product_stage" ]] || sha256sum "$product_stage"/*.c "$product_stage"/*.h "$product_stage"/*.inc "$product_stage/manifest.json" >> "$output/EXPERIMENT-SHA256SUMS"
[[ -z "$loading_stage" ]] || sha256sum "$loading_stage"/*.c "$loading_stage"/*.h "$loading_stage/manifest.json" >> "$output/EXPERIMENT-SHA256SUMS"
input_edges=${DS_INPUT_EDGE_FIX:-1}
[[ "$input_edges" == 0 || "$input_edges" == 1 ]] || exit 2
image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
docker run --rm --network none --read-only --tmpfs /tmp \
  -e DS_INPUT_EDGE_FIX="$input_edges" \
  -e DS_RT_PROXY_SOURCE="$proxy_source" -e DS_RT_OUTPUT="$output_subdir" -e DS_RT_GEOMETRY_SOURCE="$geometry_source" \
  -e DS_RT_PROFILER_SOURCE="$profiler_source" \
  -e DS_RT_SKINNING_SOURCE="$skinning_source" \
  -e DS_RT_RENDERER_SOURCE="$renderer_source" \
  -e DS_RT_D3D8_SOURCE="$facade_source" \
  -e DS_RT_ROTATION_STAGE="$rotation_stage" \
  -e DS_RT_LENGTH_STAGE="$length_stage" \
  -e DS_RT_PHYSICS_PRODUCT_STAGE="$product_stage" \
  -e DS_RT_LOADING_STAGE="$loading_stage" \
  --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" \
  -v "$PWD/build/render-thread-game:/experiment:rw" --workdir /src "$image" bash -eu -o pipefail -c '
    frozen=/experiment/snapshot
    out=/experiment/$DS_RT_OUTPUT
    flags="-std=gnu11 -O1 -g -DDS_HEAP_START=0x40000000u -I/src/probe -I$frozen/runtime -I$frozen/third_party/pcrecomp/runtime/recomp32 -I$frozen/third_party/pcrecomp/runtime/win32hle -I/src/build/sdk-include -I/src/build/sdk-include/SDL2 -I/usr/include/SDL2"
    for name in d3d8 d3d8_renderer fps_overlay game_main; do
      optimize=""
      [[ "$name" != d3d8 ]] || optimize="-O2 -fno-strict-aliasing -fwrapv -ffp-contract=off -mcpu=cortex-a53"
      [[ "$name" != d3d8_renderer ]] || optimize="-O3 -mcpu=cortex-a53"
      source=/experiment/src/$name.c
      [[ "$name" != d3d8_renderer ]] || source=/src/$DS_RT_RENDERER_SOURCE
      [[ "$name" != d3d8 ]] || source=/src/$DS_RT_D3D8_SOURCE
      aarch64-linux-gnu-gcc $flags $optimize -c "$source" -o "$out/obj/ds_$name.o"
    done
    aarch64-linux-gnu-gcc -std=gnu11 -O2 -g -mcpu=cortex-a53 -Wall -Wextra \
      -I/src/probe -I/src/build/render-thread-stage -I/src/build/sdk-include -I/src/build/sdk-include/SDL2 -I/usr/include/SDL2 \
      -c "/src/$DS_RT_PROXY_SOURCE" -o "$out/obj/render_thread_proxy.o"
    rm -f "$out/obj/ds_geometry_fast.o"
    if [[ -n "$DS_RT_GEOMETRY_SOURCE" ]]; then
      aarch64-linux-gnu-gcc -std=gnu11 -O2 -g -fno-strict-aliasing -fwrapv -ffp-contract=off -mcpu=cortex-a53 \
        -I"$frozen/runtime" -I"$frozen/third_party/pcrecomp/runtime/recomp32" \
        -c "/src/$DS_RT_GEOMETRY_SOURCE" -o "$out/obj/ds_geometry_fast.o"
    fi
    rm -f "$out/obj/ds_profiler.o"
    profile_libs=()
    if [[ -n "$DS_RT_PROFILER_SOURCE" ]]; then
      aarch64-linux-gnu-gcc -std=gnu11 -O2 -g -mcpu=cortex-a53 -Wall -Wextra -Werror \
        -I"$frozen/runtime" -c "/src/$DS_RT_PROFILER_SOURCE" -o "$out/obj/ds_profiler.o"
      profile_libs+=(-lrt)
    fi
    rm -f "$out/obj/ds_skinning_fast.o"
    if [[ -n "$DS_RT_SKINNING_SOURCE" ]]; then
      aarch64-linux-gnu-gcc -std=gnu11 -O2 -g -mcpu=cortex-a53 -ffp-contract=off \
        -I"$frozen/runtime" -I"$frozen/third_party/pcrecomp/runtime/recomp32" \
        -c "/src/$DS_RT_SKINNING_SOURCE" -o "$out/obj/ds_skinning_fast.o"
    fi
    rm -f "$out/obj/ds_rotation_scalar.o"
    if [[ -n "$DS_RT_ROTATION_STAGE" ]]; then
      stage=/src/$DS_RT_ROTATION_STAGE
      mkdir -p "$out/rotation"
      # Other functions retain the original unit compile flags. Only the new
      # scalar helper disables contraction and enables explicit rounding.
      aarch64-linux-gnu-gcc -std=gnu11 -O2 -fno-strict-aliasing -fwrapv -mcpu=cortex-a53 -DRECOMP_FP_STACK_EXACT=1 -Werror=implicit-function-declaration \
        -I"$frozen/third_party/pcrecomp/runtime/recomp32" -I"$frozen/runtime" \
        -c "$stage/functions_0071.c" -o "$out/rotation/functions_0071.aarch64.o"
      aarch64-linux-gnu-gcc -std=gnu11 -O2 -fno-strict-aliasing -fwrapv -ffp-contract=off -frounding-math -mcpu=cortex-a53 -DRECOMP_FP_STACK_EXACT=1 \
        -I"$frozen/third_party/pcrecomp/runtime/recomp32" -I"$frozen/runtime" \
        -c "$stage/rotation_scalar.c" -o "$out/obj/ds_rotation_scalar.o"
      cp "$frozen/build/translated/desertstorm/lifted.aarch64.a" "$out/rotation/lifted.aarch64.a"
      aarch64-linux-gnu-ar rcs "$out/rotation/lifted.aarch64.a" "$out/rotation/functions_0071.aarch64.o"
    fi
    rm -f "$out/obj/ds_length_scalar.o"
    if [[ -n "$DS_RT_LENGTH_STAGE" ]]; then
      stage=/src/$DS_RT_LENGTH_STAGE
      mkdir -p "$out/rotation"
      aarch64-linux-gnu-gcc -std=gnu11 -O2 -fno-strict-aliasing -fwrapv -mcpu=cortex-a53 -DRECOMP_FP_STACK_EXACT=1 -Werror=implicit-function-declaration \
        -I"$frozen/third_party/pcrecomp/runtime/recomp32" -I"$frozen/runtime" \
        -c "$stage/functions_0065.c" -o "$out/rotation/functions_0065.aarch64.o"
      aarch64-linux-gnu-gcc -std=gnu11 -O2 -fno-strict-aliasing -fwrapv -ffp-contract=off -frounding-math -mcpu=cortex-a53 -DRECOMP_FP_STACK_EXACT=1 \
        -I"$frozen/third_party/pcrecomp/runtime/recomp32" -I"$frozen/runtime" \
        -c "$stage/length_scalar.c" -o "$out/obj/ds_length_scalar.o"
      if [[ -z "$DS_RT_ROTATION_STAGE" ]]; then
        cp "$frozen/build/translated/desertstorm/lifted.aarch64.a" "$out/rotation/lifted.aarch64.a"
      fi
      aarch64-linux-gnu-ar rcs "$out/rotation/lifted.aarch64.a" "$out/rotation/functions_0065.aarch64.o"
    fi
    rm -f "$out/obj/ds_physics_product.o"
    if [[ -n "$DS_RT_PHYSICS_PRODUCT_STAGE" ]]; then
      stage=/src/$DS_RT_PHYSICS_PRODUCT_STAGE
      mkdir -p "$out/rotation"
      aarch64-linux-gnu-gcc -std=gnu11 -O2 -fno-strict-aliasing -fwrapv -mcpu=cortex-a53 -DRECOMP_FP_STACK_EXACT=1 -Werror=implicit-function-declaration \
        -I"$frozen/third_party/pcrecomp/runtime/recomp32" -I"$frozen/runtime" \
        -c "$stage/functions_0097.c" -o "$out/rotation/functions_0097.aarch64.o"
      aarch64-linux-gnu-gcc -std=gnu11 -O2 -fno-strict-aliasing -fwrapv -ffp-contract=off -mcpu=cortex-a53 -DRECOMP_FP_STACK_EXACT=1 \
        -I"$frozen/third_party/pcrecomp/runtime/recomp32" -I"$frozen/runtime" \
        -c "$stage/physics_product.c" -o "$out/obj/ds_physics_product.o"
      if [[ -z "$DS_RT_ROTATION_STAGE" && -z "$DS_RT_LENGTH_STAGE" ]]; then
        cp "$frozen/build/translated/desertstorm/lifted.aarch64.a" "$out/rotation/lifted.aarch64.a"
      fi
      aarch64-linux-gnu-ar rcs "$out/rotation/lifted.aarch64.a" "$out/rotation/functions_0097.aarch64.o"
    fi
    if [[ -n "$DS_RT_LOADING_STAGE" ]]; then
      stage=/src/$DS_RT_LOADING_STAGE
      mkdir -p "$out/rotation"
      aarch64-linux-gnu-gcc -std=gnu11 -O2 -fno-strict-aliasing -fwrapv -mcpu=cortex-a53 -DRECOMP_FP_STACK_EXACT=1 -Werror=implicit-function-declaration \
        -I"$frozen/third_party/pcrecomp/runtime/recomp32" -I"$frozen/runtime" \
        -c "$stage/functions_0003.c" -o "$out/rotation/functions_0003.aarch64.o"
      if [[ -z "$DS_RT_ROTATION_STAGE" && -z "$DS_RT_LENGTH_STAGE" && -z "$DS_RT_PHYSICS_PRODUCT_STAGE" ]]; then
        cp "$frozen/build/translated/desertstorm/lifted.aarch64.a" "$out/rotation/lifted.aarch64.a"
      fi
      aarch64-linux-gnu-ar rcs "$out/rotation/lifted.aarch64.a" "$out/rotation/functions_0003.aarch64.o"
    fi
    if [[ "$DS_INPUT_EDGE_FIX" == 1 ]]; then
      aarch64-linux-gnu-gcc $flags -O2 -I/src/runtime -c /src/runtime/dinput8.c -o "$out/obj/ds_dinput8.o"
    else
      rm -f "$out/obj/ds_dinput8.o"
    fi
    objects=()
    for obj in "$frozen"/build/game/obj/*.o; do
      case "${obj##*/}" in ds_d3d8.o|ds_d3d8_renderer.o|ds_fps_overlay.o|ds_game_main.o) continue;; esac
      if [[ "${obj##*/}" == ds_dinput8.o && "$DS_INPUT_EDGE_FIX" == 1 ]]; then continue; fi
      if [[ "${obj##*/}" == ds_loading_background.o && -n "$DS_RT_LOADING_STAGE" ]]; then continue; fi
      if [[ "${obj##*/}" == ds_geometry_fast.o && -n "$DS_RT_GEOMETRY_SOURCE" ]]; then continue; fi
      if [[ "${obj##*/}" == ds_profiler.o && -n "$DS_RT_PROFILER_SOURCE" ]]; then continue; fi
      if [[ "${obj##*/}" == ds_skinning_fast.o && -n "$DS_RT_SKINNING_SOURCE" ]]; then continue; fi
      objects+=("$obj")
    done
    objects+=("$out"/obj/*.o)
    mapfile -t names < <(aarch64-linux-gnu-nm -g --defined-only "$out/obj/render_thread_proxy.o" | awk '\''$3 ~ /^__wrap_/ {sub(/^__wrap_/,"",$3);print $3}'\'' | sort -u)
    test "${#names[@]}" -gt 0
    printf "%s\n" "${names[@]}" > "$out/wrapped-api.txt"
    aarch64-linux-gnu-nm -u "${objects[@]}" | awk '\''$NF ~ /^gl[A-Z]/ {print $NF}'\'' | sort -u > "$out/required-gl-api.txt"
    comm -23 "$out/required-gl-api.txt" "$out/wrapped-api.txt" > "$out/missing-gl-api.txt"
    if [[ -s "$out/missing-gl-api.txt" ]]; then cat "$out/missing-gl-api.txt"; exit 1; fi
    wrappers=();for name in "${names[@]}"; do wrappers+=("-Wl,--wrap=$name"); done
    archives=();for module in "$frozen"/build/translated/*; do
      [[ -d "$module" ]] || continue
      if [[ "${module##*/}" == desertstorm && ( -n "$DS_RT_ROTATION_STAGE" || -n "$DS_RT_LENGTH_STAGE" || -n "$DS_RT_PHYSICS_PRODUCT_STAGE" || -n "$DS_RT_LOADING_STAGE" ) ]]; then
        archives+=("$out/rotation/lifted.aarch64.a")
      else archives+=("$module/lifted.aarch64.a");fi
    done
    aarch64-linux-gnu-gcc -g -rdynamic -o "$out/game-render-thread.aarch64.new" \
      "${objects[@]}" "${wrappers[@]}" -Wl,--start-group "${archives[@]}" -Wl,--end-group \
      -L"$frozen/build/target-libs" -Wl,--allow-shlib-undefined -lSDL2 -lSDL2_ttf -lGLESv2 -lm -lpthread -ldl "${profile_libs[@]}"
    mv "$out/game-render-thread.aarch64.new" "$out/game-render-thread.aarch64"
  '
sha256sum -c "$output/EXPERIMENT-SHA256SUMS"
readelf -h "$output/game-render-thread.aarch64" > "$output/elf-header.txt"
readelf -d "$output/game-render-thread.aarch64" > "$output/dynamic.txt"
sha256sum "$output/game-render-thread.aarch64" > "$output/SHA256SUMS"
