#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/game
python3 - <<'PY'
from pathlib import Path
import re,pefile
pe=pefile.PE('setup_conflict_-_desert_storm_1.0_(21657)/DesertStorm.exe')
Path('build/game/memmove-image.bin').write_bytes(pe.get_memory_mapped_image().ljust(pe.OPTIONAL_HEADER.SizeOfImage,b'\0'))
addresses={0x4508f0,0x450770,0x40a110,0x539dd0,0x539da0,0x539e10,0x539820,0x44efb0,0x44f000,0x478440}
out='#define RECOMP_GENERATED_CODE\n#include "functions.h"\n'
for path in Path('build/translated/desertstorm').glob('functions_*.c'):
    source=path.read_text()
    for address in list(addresses):
        match=re.search(r'^void recomp_desertstorm_%08X\(void\) \{.*?(?=^void |\Z)'%address,source,re.M|re.S)
        if match:out+=match.group()+'\n';addresses.remove(address)
if addresses:raise SystemExit(str(addresses))
Path('build/game/menu-forward-reference.c').write_text(out)
PY
"${CC:-cc}" -std=gnu11 -O2 -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 -Ibuild/translated/desertstorm \
    tests/menu_forward.c build/game/menu-forward-reference.c -lm -o build/game/menu-forward-selftest
build/game/menu-forward-selftest
