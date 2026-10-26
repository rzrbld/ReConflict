#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
PYTHONDONTWRITEBYTECODE=1 python3 scripts/recover_targets.py
python3 - <<'PY'
from pathlib import Path
import re
import pefile
pe=pefile.PE('setup_conflict_-_desert_storm_1.0_(21657)/DesertStorm.exe')
image=pe.get_memory_mapped_image()
image=image.ljust(pe.OPTIONAL_HEADER.SizeOfImage,b'\0')
Path('build/game/memmove-image.bin').write_bytes(image)
directory=Path('build/translated/desertstorm')
name='recomp_desertstorm_00568A70'
for path in directory.glob('functions_*.c'):
    source=path.read_text()
    match=re.search(r'^void '+name+r'\(void\) \{.*?(?=^void |\Z)',source,re.M|re.S)
    if match:
        Path('build/game/memmove-reference.c').write_text('#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\n'+match.group(0))
        break
else:
    raise SystemExit('Original memory-copy function not found in generated source')
PY
"${CC:-cc}" -std=gnu11 -O1 -Werror=implicit-function-declaration \
    -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 -Ithird_party/pcrecomp/runtime/win32hle \
    tests/recovered_memmove.c build/game/recovered.c build/game/memmove-reference.c \
    -lm -o build/game/recovered-memmove-test
build/game/recovered-memmove-test
