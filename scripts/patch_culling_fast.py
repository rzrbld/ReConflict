#!/usr/bin/env python3
"""Review/apply the opt-in exact0x50C359 prefix continuation hook in generated C only."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[1]
ADDRESS=0x50c350
DIGEST='d712f5ec82c14da66b098332b3cc9c99689091f46317112ea35dfd393dc3e164'
ENTRY=bytes.fromhex('83ec18535556578bf1d94608a1244064')
HEADER='#include "culling_fast.h"'
FUNCTION='void recomp_desertstorm_0050C350(void) {'
OLD_HOOK='    if(ds_culling_fast_50c350())return; /* opt-in differential-tested node rejection */'
HOOK='    {int path=ds_culling_prefix_50c359();if(path){RECOMP_FLAGS_IN();if(path==1)goto L_0050C40F;goto L_0050C48A;}}'
BODY_DIGEST='b3efaf7af04e903deb758d4054ce73d7c45b3e8603c9872618d592a6cfd384f3'

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
    raise ValueError('Culling entry is not file-backed')

def patched_source(source):
    if source.count(FUNCTION)!=1:raise ValueError('Expected exactly one original0x50c350 function')
    if source.count('#include "functions.h"')!=1:raise ValueError('Unexpected generated include anchor')
    match=re.search(r'^void recomp_desertstorm_0050C350\(void\) \{.*?(?=^void |\Z)',source,re.M|re.S)
    if not match:raise ValueError('Original culling body missing')
    body=match.group()
    if body.count(OLD_HOOK)>1 or body.count(HOOK)>1:raise ValueError('Duplicated culling hook')
    plain=body.replace(OLD_HOOK+'\n','').replace(HOOK+'\n','')
    if 'ds_culling_' in plain:raise ValueError('Unexpected culling hook spelling')
    if hashlib.sha256(plain.encode()).hexdigest()!=BODY_DIGEST:raise ValueError('Original unhooked culling body digest mismatch')
    if plain.count('L_0050C359:\n')!=1:raise ValueError('Prefix label is not unique within checked body')
    replacement=plain.replace('L_0050C359:\n','L_0050C359:\n'+HOOK+'\n',1)
    source=source[:match.start()]+replacement+source[match.end():]
    if HEADER not in source:source=source.replace('#include "functions.h"','#include "functions.h"\n'+HEADER,1)
    elif source.count(HEADER)!=1:raise ValueError('Duplicated culling include')
    return source

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply',action='store_true',help='Explicitly change the generated translation unit')
    args=parser.parse_args()
    original=ROOT/'setup_conflict_-_desert_storm_1.0_(21657)/DesertStorm.exe'
    data=original.read_bytes();digest=hashlib.sha256(data).hexdigest()
    if digest!=DIGEST:raise SystemExit('Unsupported original binary SHA256')
    if entry_bytes(data)!=ENTRY:raise SystemExit('Original culling entry byte anchor mismatch')
    path=ROOT/'build/translated/desertstorm/functions_0061.c'
    source=path.read_text();changed=patched_source(source)
    if args.apply and changed!=source:
        temporary=path.with_name(f'.{path.name}.{os.getpid()}.tmp')
        try:temporary.write_text(changed);temporary.replace(path)
        finally:temporary.unlink(missing_ok=True)
    print(json.dumps({'binary_sha256':digest,'entry':hex(ADDRESS),'entry_bytes':ENTRY.hex(),
                      'unit':str(path.relative_to(ROOT)),'applied':args.apply,'would_change':changed!=source,
                      'enable':'DS_FAST_CULLING=1','fallback':'Unchanged lifted function when disabled or declined'},indent=2))

if __name__=='__main__':main()
