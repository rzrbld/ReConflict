#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
translated_opt=${TRANSLATED_OPT:-2}
[[ "$translated_opt" =~ ^[123]$ ]] || { echo 'TRANSLATED_OPT must be 1, 2 or 3' >&2; exit 1; }
fp_stack_exact=${RECOMP_FP_STACK_EXACT:-1}
[[ "$fp_stack_exact" =~ ^[01]$ ]] || { echo 'RECOMP_FP_STACK_EXACT must be 0 or 1' >&2; exit 1; }
# These helpers are generated from local game inputs and excluded from Git.
python3 scripts/generate_geometry_fast.py --apply
python3 scripts/generate_quaternion_fast.py --apply
python3 scripts/generate_matrix_inverse_fast.py --apply
python3 scripts/generate_physics_fast.py --apply
python3 scripts/apply_offline_policy.py
python3 scripts/patch_nonlocal.py
python3 scripts/patch_math_fast.py --apply
python3 scripts/patch_matrix_multiply_fast.py --apply
python3 scripts/patch_skinning_fast.py --apply
python3 scripts/patch_culling_fast.py --apply
python3 scripts/patch_audio_decode_fast.py --apply
python3 scripts/patch_matrix_inverse_fast.py --apply
python3 scripts/patch_geometry_fast.py --apply
python3 scripts/patch_quaternion_fast.py --apply
python3 scripts/generate_physics_fast.py
python3 scripts/patch_physics_fast.py --apply
python3 scripts/patch_jinvmjt_fast.py --apply
docker run --rm --network none --read-only --tmpfs /tmp \
  --user "$(id -u):$(id -g)" -e DS_TRANSLATED_OPT="$translated_opt" -e DS_FP_STACK_EXACT="$fp_stack_exact" \
  -v "$PWD:/src:ro" -v "$PWD/build/translated:/out:rw" \
  --workdir /src "$image" bash -eu -o pipefail -c '
    export LC_ALL=C
    export DS_TRANSLATED_FLAGS="-O${DS_TRANSLATED_OPT} -fno-strict-aliasing -fwrapv -mcpu=cortex-a53 -DRECOMP_FP_STACK_EXACT=${DS_FP_STACK_EXACT}"
    printf "Translated flags: %s\n" "$DS_TRANSLATED_FLAGS"
    find /out -name "*.c" -print0 | sort -z | xargs -0 -P 4 -I {} bash -c '\''
      file=$1
      obj=${file%.c}.aarch64.o
      file_flags=$DS_TRANSLATED_FLAGS
      [[ "$file" == /out/desertstorm/functions_0097.c ]] && file_flags="$file_flags -ffp-contract=off"
      # GCC12 spends disproportionate time optimizing this large mission
      # script dispatcher. Keep its proven O1 code; the engine and other
      # mission units use the requested optimization level.
      [[ "$file" == /out/mission8/functions_0000.c ]] && file_flags=${file_flags/-O${DS_TRANSLATED_OPT}/-O1}
      if [[ -s "$obj" && -f "$obj.flags" && "$(cat "$obj.flags")" == "$file_flags" && "$obj" -nt "$file" && "$obj" -nt "$(dirname "$file")/functions.h" &&
            "$obj" -nt /src/runtime/cpu_features.h && "$obj" -nt /src/runtime/fp_stack_exact.h &&
            "$obj" -nt /src/third_party/pcrecomp/runtime/recomp32/recomp_types.h &&
            ( "$file" != /out/desertstorm/functions_0066.c || "$obj" -nt /src/runtime/skinning_fast.h ) &&
            ( "$file" != /out/desertstorm/functions_0061.c || "$obj" -nt /src/runtime/culling_fast.h ) &&
            ( "$file" != /out/desertstorm/functions_0078.c || "$obj" -nt /src/runtime/audio_decode_fast.h ) &&
            ( "$file" != /out/desertstorm/functions_0038.c || "$obj" -nt /src/runtime/matrix_inverse_fast.h ) &&
            ( "$file" != /out/desertstorm/functions_0071.c || "$obj" -nt /src/runtime/quaternion_fast.h ) &&
            ( "$file" != /out/desertstorm/functions_0097.c || ( "$obj" -nt /src/runtime/physics_fast.h && "$obj" -nt /src/runtime/physics_rank_update.inc && "$obj" -nt /src/runtime/jinvmjt_fast.h ) ) &&
            ( ( "$file" != /out/desertstorm/functions_0042.c && "$file" != /out/desertstorm/functions_0072.c ) || "$obj" -nt /src/runtime/geometry_fast.h ) ]]; then exit 0; fi
      aarch64-linux-gnu-gcc -std=gnu11 $file_flags -Werror=implicit-function-declaration \
        -I/src/third_party/pcrecomp/runtime/recomp32 -I/src/runtime -c "$file" -o "${file%.c}.aarch64.o" \
        > "${file%.c}.compile.log" 2>&1
      status=$?
      [[ $status -eq 0 ]] || exit "$status"
      printf "%s\n" "$file_flags" > "$obj.flags"
    '\'' bash {}
    for module in /out/*; do
      test -d "$module" || continue
      aarch64-linux-gnu-ar rcs "$module/lifted.aarch64.a" "$module"/*.aarch64.o
    done
    aarch64-linux-gnu-gcc -std=gnu11 -O2 -c /src/runtime/cpu_features.c -o /out/cpu_features.aarch64.o
    aarch64-linux-gnu-gcc -std=gnu11 -O2 -c /src/runtime/unsupported.c -o /out/unsupported.aarch64.o
    aarch64-linux-gnu-ar rcs /out/support.aarch64.a /out/cpu_features.aarch64.o /out/unsupported.aarch64.o
  '
