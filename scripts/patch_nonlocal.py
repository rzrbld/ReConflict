#!/usr/bin/env python3
"""Install checked host setjmp frames into the 3 actual guest callsites.

Only generated C changes. The original PE/asset files remain read only.
Run after translate_game.py and before recompiling the changed units.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import pefile

root=Path(__file__).resolve().parents[1]
binary=root/'setup_conflict_-_desert_storm_1.0_(21657)/DesertStorm.exe'
digest=hashlib.sha256(binary.read_bytes()).hexdigest()
if digest!='d712f5ec82c14da66b098332b3cc9c99689091f46317112ea35dfd393dc3e164':raise SystemExit('Unsupported game hash')
pe=pefile.PE(str(binary));image=pe.get_memory_mapped_image();base=pe.OPTIONAL_HEADER.ImageBase
sites={0x5a0209:0x5a0248,0x5a0bf6:0x5a0c60,0x5b3592:0x5b35a9}
for owner,site in sites.items():
    raw=image[site-base:site-base+5]
    if raw[0]!=0xe8 or site+5+int.from_bytes(raw[1:],'little',signed=True)!=0x5c9654:raise SystemExit(f'Callsite mismatch at {site:#x}')
changed=[];affected=set();handled=set();longjmp_handled=False
for path in sorted((root/'build/translated/desertstorm').glob('functions_*.c')):
    source=path.read_text();original=source
    for owner,site in sites.items():
        name=f'recomp_desertstorm_{owner:08X}'
        pattern=r'^void '+name+r'\(void\) \{.*?(?=^void |\Z)'
        match=re.search(pattern,source,re.M|re.S)
        if not match:continue
        affected.add(str(path.relative_to(root)))
        body=match.group(0);handled.add(owner)
        if 'DS_GUEST_SETJMP(' not in body:
            enter=f'    RECOMP_ENTER(0x{owner:08X}u);'
            if body.count(enter)!=1:raise SystemExit(f'Entry shape changed for {name}')
            body=body.replace(enter,enter+f'\n    DS_JmpFrame _ds_jump;\n    ds_jmp_enter(&_ds_jump,0x{owner:08X}u);',1)
            call=f'RECOMP_CALL(recomp_desertstorm_005C9654); /* 0x{site:08X}: call 0x5c9654 */'
            if body.count(call)!=1:raise SystemExit(f'Setjmp shape changed for {name}')
            body=body.replace(call,f'DS_GUEST_SETJMP(_ds_jump,recomp_desertstorm_005C9654); /* checked guest _setjmp3 at {site:#x} */')
            returns=re.findall(r'RECOMP_FLAGS_OUT\(\); esp \+= \d+; return;',body)
            if len(returns)!=1 or body.count('return;')!=1:raise SystemExit(f'Unreviewed exit shape for {name}')
            body=body.replace(returns[0],'ds_jmp_leave(&_ds_jump); '+returns[0])
            source=source[:match.start()]+body+source[match.end():]
    name='recomp_desertstorm_005C95D8'
    match=re.search(r'^void '+name+r'\(void\) \{.*?(?=^void |\Z)',source,re.M|re.S)
    if match:
        longjmp_handled=True
        affected.add(str(path.relative_to(root)))
        body='void '+name+'(void) {\n    g_cur_func=0x005C95D8u;\n    ds_guest_longjmp();\n}\n'
        source=source[:match.start()]+body+source[match.end():]
    if source!=original:
        if '#include "nonlocal.h"' not in source:source=source.replace('#include "functions.h"','#include "functions.h"\n#include "nonlocal.h"',1)
        temp=path.with_name('.'+path.name+f'.{os.getpid()}.tmp');temp.write_text(source);temp.replace(path)
        changed.append(str(path.relative_to(root)))
if handled!=set(sites) or not longjmp_handled:raise SystemExit('Missing generated callers/longjmp body')
report={'binary_sha256':digest,'callers':{hex(k):hex(v) for k,v in sites.items()},'longjmp':'0x5c95d8','affected_units':sorted(affected),'changed_units':changed,'semantics':'Host setjmp lives in translated caller; original guest environment preserved; real native nonlocal unwind','limitations':['Custom guest SEH unwind callbacks and changing SEH chains fail explicitly.']}
(root/'docs/evidence/nonlocal-patch.json').write_text(json.dumps(report,indent=2)+'\n')
print('Checked 3 setjmp callers and guest longjmp; changed '+str(len(changed))+' generated units')
for p in changed:print(p)
