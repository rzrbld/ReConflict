#!/usr/bin/env python3
"""Review/apply the opt-in0x529520 native skinning hook in generated C only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[1]
ADDRESS=0x529520
DIGEST='d712f5ec82c14da66b098332b3cc9c99689091f46317112ea35dfd393dc3e164'
ENTRY=bytes.fromhex('83ec748b015355894c24188b48105633')
HEADER='#include "skinning_fast.h"'
FUNCTION='void recomp_desertstorm_00529520(void) {'
HOOK='    if(ds_skinning_fast_529520())return; /* opt-in differential-tested skinning loop */'

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
    raise ValueError('Skinning entry is not file-backed')

def patched_source(source):
    if source.count(FUNCTION)!=1:raise ValueError('Expected exactly one original0x529520 function')
    if source.count('#include "functions.h"')!=1:raise ValueError('Unexpected generated include anchor')
    occurrences=source.count('ds_skinning_fast_529520()')
    if occurrences and (occurrences!=1 or FUNCTION+'\n'+HOOK not in source):
        raise ValueError('Unexpected or duplicated skinning fast-path hook')
    if not occurrences:source=source.replace(FUNCTION,FUNCTION+'\n'+HOOK,1)
    if HEADER not in source:source=source.replace('#include "functions.h"','#include "functions.h"\n'+HEADER,1)
    elif source.count(HEADER)!=1:raise ValueError('Duplicated skinning include')
    return source

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply',action='store_true',help='Explicitly change the generated translation unit')
    args=parser.parse_args()
    original=ROOT/'setup_conflict_-_desert_storm_1.0_(21657)/DesertStorm.exe'
    data=original.read_bytes();digest=hashlib.sha256(data).hexdigest()
    if digest!=DIGEST:raise SystemExit('Unsupported original binary SHA256')
    if entry_bytes(data)!=ENTRY:raise SystemExit('Original skinning entry byte anchor mismatch')
    path=ROOT/'build/translated/desertstorm/functions_0066.c'
    source=path.read_text();changed=patched_source(source)
    if args.apply and changed!=source:
        temporary=path.with_name(f'.{path.name}.{os.getpid()}.tmp')
        try:temporary.write_text(changed);temporary.replace(path)
        finally:temporary.unlink(missing_ok=True)
    print(json.dumps({'binary_sha256':digest,'entry':hex(ADDRESS),'entry_bytes':ENTRY.hex(),
                      'unit':str(path.relative_to(ROOT)),'applied':args.apply,'would_change':changed!=source,
                      'enable':'DS_FAST_MATH=1','fallback':'Unchanged lifted function when disabled or declined'},indent=2))

if __name__=='__main__':main()
