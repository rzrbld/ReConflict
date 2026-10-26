#!/usr/bin/env bash
set -euo pipefail
file=$1
flags='-std=gnu11 -flto=auto -fno-strict-aliasing -fwrapv -mcpu=cortex-a53 -DRECOMP_FP_STACK_EXACT=1 -Werror=implicit-function-declaration'
[[ "$file" == translated/desertstorm/functions_0097.c ]] && flags="$flags -ffp-contract=off"
opt=-O2
[[ "$file" == translated/mission8/functions_0000.c ]] && opt=-O1
if [[ "$file" == translated/* ]]; then
  obj="/out/${file#translated/}"
  obj=${obj%.c}.aarch64.o
else
  obj="/out/support/$(basename "${file%.c}").aarch64.o"
fi
mkdir -p "$(dirname "$obj")"
dep=${obj%.o}.d
started=$(date +%s%N)
printf '%s\n' "$flags $opt" > "$obj.flags"
if [[ -x /usr/bin/time ]]; then
  /usr/bin/time -f 'wall_seconds=%e max_rss_kib=%M' -o "$obj.time" \
    aarch64-linux-gnu-gcc $flags "$opt" -I/input/third_party/pcrecomp/runtime/recomp32 -I/input/runtime \
    -MMD -MF "$dep" -MT "$obj" -c "/input/$file" -o "$obj.new" > "$obj.compile.log" 2>&1
else
  aarch64-linux-gnu-gcc $flags "$opt" -I/input/third_party/pcrecomp/runtime/recomp32 -I/input/runtime \
    -MMD -MF "$dep" -MT "$obj" -c "/input/$file" -o "$obj.new" > "$obj.compile.log" 2>&1
fi
mv "$obj.new" "$obj"
printf '%s\n' "${LTO_RUN_ID:?missing build run ID}" > "${obj%.o}.done"
finished=$(date +%s%N)
printf 'start_unix_ns=%s end_unix_ns=%s elapsed_ns=%s\n' "$started" "$finished" "$((finished-started))" >> "$obj.time"
printf 'DONE %s\n' "$file"
