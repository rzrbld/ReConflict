#!/usr/bin/env python3
"""Review/apply opt-in matrix fast-path hooks to generated C only."""
import argparse,hashlib,json,os,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--apply',action='store_true');args=parser.parse_args()
digest=hashlib.sha256((root/'setup_conflict_-_desert_storm_1.0_(21657)/DesertStorm.exe').read_bytes()).hexdigest()
if digest!='d712f5ec82c14da66b098332b3cc9c99689091f46317112ea35dfd393dc3e164':raise SystemExit('Unsupported original binary')
path=root/'build/translated/desertstorm/functions_0072.c';source=path.read_text();original=source
for address in [0x540470,0x5405a0]:
    name=f'recomp_desertstorm_{address:08X}';entry=f'void {name}(void) {{'
    if source.count(entry)!=1:raise SystemExit(f'Expected sole {name}')
    hook=f'    if(ds_math_fast_{address:x}())return; /* opt-in verified matrix fast path */'
    if hook not in source:source=source.replace(entry,entry+'\n'+hook,1)
if '#include "math_fast.h"' not in source:source=source.replace('#include "functions.h"','#include "functions.h"\n#include "math_fast.h"',1)
if args.apply and source!=original:
    temp=path.with_name(f'.{path.name}.{os.getpid()}.tmp');temp.write_text(source);temp.replace(path)
report={'binary_sha256':digest,'unit':str(path.relative_to(root)),'applied':args.apply,'would_change':source!=original,'enable':'DS_FAST_MATH=1','fallback':'Original lifted functions for unsupported cases; default disabled'}
print(json.dumps(report,indent=2))
