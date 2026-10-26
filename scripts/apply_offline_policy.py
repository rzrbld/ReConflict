#!/usr/bin/env python3
"""Apply a checked single-player-only policy to generated C, not original PE.

At 0x40b762..0x40b7a7 the original game loads dpnhpast.dll, tests presence and
frees it. No export is invoked. On a native non-Windows port this install
probe does not describe available graphics/input; bypass it explicitly while
leaving actual DirectPlay calls unresolved/unsupported.
"""
import hashlib
from pathlib import Path
import pefile

root=Path(__file__).resolve().parents[1]
binary=root/'setup_conflict_-_desert_storm_1.0_(21657)'/'DesertStorm.exe'
expected='d712f5ec82c14da66b098332b3cc9c99689091f46317112ea35dfd393dc3e164'
if hashlib.sha256(binary.read_bytes()).hexdigest()!=expected:raise SystemExit('Unexpected original executable SHA256')
pe=pefile.PE(str(binary))
if pe.get_data(0x40b76f-pe.OPTIONAL_HEADER.ImageBase,2)!=bytes.fromhex('7530'):raise SystemExit('Install-probe branch mismatch')
if pe.get_data(0x5e77fc-pe.OPTIONAL_HEADER.ImageBase,13)!=b'dpnhpast.dll\0':raise SystemExit('Install-probe DLL name mismatch')
old='if (TEST_NZ(_flag_a, _flag_b)) goto L_0040B7A1; /* 0x0040B76F: jne 0x40b7a1 */'
new='if (ds_offline_skip_directplay_install_probe() || TEST_NZ(_flag_a, _flag_b)) goto L_0040B7A1; /* 0x0040B76F: checked single-player install-probe policy */'
changed=[]
for source in (root/'build/translated/desertstorm').glob('functions_*.c'):
    text=source.read_text()
    if old not in text and new not in text:continue
    if '#include "offline_network.h"' not in text:text=text.replace('#include "functions.h"','#include "functions.h"\n#include "offline_network.h"')
    text=text.replace(old,new)
    release='''    PUSH32(esp, eax); /* 0x0040B7A1: push eax */
    RECOMP_ICALL(MEM32(0x5D8074)); /* 0x0040B7A2: call dword ptr [0x5d8074] */
    RECOMP_FLAGS_IN();'''
    guarded='''    if (eax) { /* Native single-player policy never fabricates a DLL handle. */
    PUSH32(esp, eax); /* 0x0040B7A1: push eax */
    RECOMP_ICALL(MEM32(0x5D8074)); /* 0x0040B7A2: call dword ptr [0x5d8074] */
    RECOMP_FLAGS_IN();
    }'''
    if guarded not in text:
        if release not in text:raise SystemExit('Expected corresponding FreeLibrary call was not found')
        text=text.replace(release,guarded)
    if source.read_text()!=text:source.write_text(text)
    changed.append(str(source.relative_to(root)))
if not changed:raise SystemExit('No expected install-probe instruction in generated C')
print('Checked offline DirectPlay install-probe policy applied to '+', '.join(changed))
