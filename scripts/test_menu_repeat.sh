#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/game
python3 - <<'PY'
from pathlib import Path
import re
source=Path('build/translated/desertstorm/functions_0026.c').read_text()
match=re.search(r'^void recomp_desertstorm_00478530\(void\) \{.*?(?=^void |\Z)',source,re.M|re.S)
if not match:raise SystemExit('Original menu up callback missing')
Path('build/game/menu-repeat-reference.c').write_text('#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\n'+match.group(0))
PY
"${CC:-cc}" -std=gnu11 -O1 -Ithird_party/pcrecomp/runtime/recomp32 \
    tests/menu_repeat.c build/game/menu-repeat-reference.c -lm -o build/game/menu-repeat-selftest
build/game/menu-repeat-selftest
