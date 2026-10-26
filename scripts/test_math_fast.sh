#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/game
python3 - <<'PY'
from pathlib import Path
import re
source=Path('build/translated/desertstorm/functions_0072.c').read_text()
out='#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\n'
for address in [0x540470,0x5405a0]:
    name=f'recomp_desertstorm_{address:08X}'
    match=re.search(r'^void '+name+r'\(void\) \{.*?(?=^void |\Z)',source,re.M|re.S)
    if not match:raise SystemExit('Missing original matrix helper')
    body=re.sub(r'^    if\(ds_math_fast_.*?\n','',match.group(0),flags=re.M)
    out+=body.replace(name,f'reference_{address:08X}')+'\n'
Path('build/game/math-reference.c').write_text(out)
PY
"${CC:-cc}" -std=gnu11 -O2 -ffp-contract=off -Werror=implicit-function-declaration \
    -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 \
    tests/math_fast.c runtime/math_fast.c build/game/math-reference.c -lm -o build/game/math-fast-selftest
build/game/math-fast-selftest
