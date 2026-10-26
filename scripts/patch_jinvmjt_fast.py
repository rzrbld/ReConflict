#!/usr/bin/env python3
"""Review/apply the opt-in0x595270 native matrix hook in generated C only."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[1]
ADDRESS=0x595270
DIGEST='d712f5ec82c14da66b098332b3cc9c99689091f46317112ea35dfd393dc3e164'
ENTRY=bytes.fromhex('558bec83ec5c5356576a008bd9680c2d')
HEADER='#include "jinvmjt_fast.h"'
FUNCTION='void recomp_desertstorm_00595270(void) {'
HOOKS={
    'L_00595361:':'    if(ds_jinvmjt_fast_kernel(1)){RECOMP_FLAGS_IN();goto L_0059541E;}',
    'L_005953D3:':'    if(ds_jinvmjt_fast_kernel(0)){RECOMP_FLAGS_IN();goto L_0059541E;}',
}
BODY_DIGEST='5634df712c10db0cbd4b3af465f0a1ba5e0fa126e151a8988284b12f6801f1c6'

def entry_bytes(data):
    """Map the fixed guest entry through the original PE section table."""
    pe=struct.unpack_from('<I',data,0x3c)[0]
    if data[:2]!=b'MZ' or data[pe:pe+4]!=b'PE\0\0':raise ValueError('Expected original PE')
    sections=struct.unpack_from('<H',data,pe+6)[0]
    optional=pe+24;size=struct.unpack_from('<H',data,pe+20)[0]
    if struct.unpack_from('<H',data,optional)[0]!=0x10b:raise ValueError('Expected PE32')
    base=struct.unpack_from('<I',data,optional+28)[0];rva=ADDRESS-base
    for i in range(sections):
        s=optional+size+i*40
        virtual_size,virtual,raw_size,raw=struct.unpack_from('<IIII',data,s+8)
        if virtual<=rva and rva+len(ENTRY)<=virtual+raw_size:
            start=raw+rva-virtual;return data[start:start+len(ENTRY)]
    raise ValueError('Matrix entry is not file-backed')

def patched_source(source):
    if source.count(FUNCTION)!=1:raise ValueError('Expected exactly one original0x595270 function')
    if source.count('#include "functions.h"')!=1:raise ValueError('Unexpected generated include anchor')
    match=re.search(r'^void recomp_desertstorm_00595270\(void\) \{.*?(?=^void |\Z)',source,re.M|re.S)
    if not match:raise ValueError('Original595270 missing')
    plain=match.group()
    for hook in HOOKS.values():
        if plain.count(hook)>1:raise ValueError('Duplicated Jacobian hook')
        plain=plain.replace(hook+'\n','')
    if 'ds_jinvmjt_' in plain:raise ValueError('Unexpected Jacobian hook')
    if hashlib.sha256(plain.encode()).hexdigest()!=BODY_DIGEST:raise ValueError('Original unhooked595270 body mismatch')
    body=plain
    for label,hook in HOOKS.items():
        if body.count(label+'\n')!=1:raise ValueError('Expected unique bounded kernel label')
        body=body.replace(label+'\n',label+'\n'+hook+'\n',1)
    source=source[:match.start()]+body+source[match.end():]
    if HEADER not in source:source=source.replace('#include "functions.h"','#include "functions.h"\n'+HEADER,1)
    elif source.count(HEADER)!=1:raise ValueError('Duplicated matrix include')
    return source

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply',action='store_true',help='Explicitly change the generated translation unit')
    args=parser.parse_args()
    original=ROOT/'setup_conflict_-_desert_storm_1.0_(21657)/DesertStorm.exe'
    data=original.read_bytes();digest=hashlib.sha256(data).hexdigest()
    if digest!=DIGEST:raise SystemExit('Unsupported original binary SHA256')
    if entry_bytes(data)!=ENTRY:raise SystemExit('Original matrix entry byte anchor mismatch')
    path=ROOT/'build/translated/desertstorm/functions_0097.c'
    source=path.read_text();changed=patched_source(source)
    if args.apply and changed!=source:
        temporary=path.with_name(f'.{path.name}.{os.getpid()}.tmp')
        try:temporary.write_text(changed);temporary.replace(path)
        finally:temporary.unlink(missing_ok=True)
    print(json.dumps({'binary_sha256':digest,'entry':hex(ADDRESS),'entry_bytes':ENTRY.hex(),
                      'unit':str(path.relative_to(ROOT)),'applied':args.apply,'would_change':changed!=source,
                      'enable':'DS_FAST_MATH=1','fallback':'Unchanged lifted function when disabled or declined'},indent=2))

if __name__=='__main__':main()
