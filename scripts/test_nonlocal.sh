#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
PYTHONDONTWRITEBYTECODE=1 python3 scripts/patch_nonlocal.py
python3 - <<'PY'
from pathlib import Path
import re
for path in Path('build/translated/desertstorm').glob('functions_*.c'):
    match=re.search(r'^void recomp_desertstorm_005C9654\(void\) \{.*?(?=^void |\Z)',path.read_text(),re.M|re.S)
    if match:
        Path('build/game/setjmp-reference.c').write_text('#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\n'+match.group(0))
        break
else:raise SystemExit('Original _setjmp3 body missing')
PY
"${CC:-cc}" -std=gnu11 -O1 -Werror=implicit-function-declaration \
    -Iruntime -Ithird_party/pcrecomp/runtime/win32hle -Ithird_party/pcrecomp/runtime/recomp32 \
    tests/nonlocal_guest.c runtime/nonlocal.c build/game/setjmp-reference.c \
    -lm -pthread -o build/game/nonlocal-selftest
build/game/nonlocal-selftest
