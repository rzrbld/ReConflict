#!/usr/bin/env bash
# Differential original guest state, not just mathematical output.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/game
python3 - <<'PY'
from pathlib import Path
import re,hashlib
original=Path('setup_conflict_-_desert_storm_1.0_(21657)/DesertStorm.exe')
if hashlib.sha256(original.read_bytes()).hexdigest()!='d712f5ec82c14da66b098332b3cc9c99689091f46317112ea35dfd393dc3e164':raise SystemExit('Unsupported original binary')
s=Path('build/translated/desertstorm/functions_0039.c').read_text()
m=re.search(r'^void recomp_desertstorm_004AC1FB\(void\) \{.*?(?=^void |\Z)',s,re.M|re.S)
if not m:raise SystemExit('Original matrix function missing')
body=re.sub(r'^    if\(ds_matrix_multiply_fast_4ac1fb\(\)\)return;.*\n','',m.group(),flags=re.M)
body=body.replace('recomp_desertstorm_004AC1FB','reference_004AC1FB')
Path('build/game/matrix-multiply-reference.c').write_text('#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\n'+body)
PY
"${CC:-cc}" -std=gnu11 -O2 -fno-strict-aliasing -fwrapv -ffp-contract=off -Wall -Wextra -Wno-unused-label -Wno-unused-variable \
    -Werror=implicit-function-declaration -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 \
    tests/matrix_multiply_fast.c runtime/matrix_multiply_fast.c build/game/matrix-multiply-reference.c \
    -lm -o build/game/matrix-multiply-fast-selftest
env -u DS_FAST_MATH build/game/matrix-multiply-fast-selftest --disabled
DS_FAST_MATH=0 build/game/matrix-multiply-fast-selftest --disabled
DS_FAST_MATH=1 build/game/matrix-multiply-fast-selftest
