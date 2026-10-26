#!/usr/bin/env python3
"""Review or apply checked opt-in geometry hooks; preserve original fallback."""
import argparse,hashlib,json,os,re,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SHA='d712f5ec82c14da66b098332b3cc9c99689091f46317112ea35dfd393dc3e164'
SPECS={'004BB500':('0042','c3e9cbc65537008669dea44222bce00d0e3bef41e4a73b1dd230413d15b5e362','83ec1c8bc18b4c24208bd156c744241c'),'005409A0':('0072','d125c0e02ffbe7ae6f67c9ae437a32951ca150c7b6a1d83cc18ac894f88674b5','8b414083ec0c83f8010f8465010000a8')}
def patched_source(source,va,digest):
 entry=f'void recomp_desertstorm_{va}(void) {{';hook=f'    if(ds_geometry_fast_{va.lower()}())return; /* opt-in verified geometry fast path */'
 if source.count(entry)!=1:raise ValueError('Expected exactly one entry '+va)
 match=re.search(r'^void recomp_desertstorm_'+va+r'\(void\) \{.*?(?=^void |\Z)',source,re.M|re.S);body=match.group()
 if 'ds_geometry_fast_' in body:
  if body.count(hook)!=1 or not body.startswith(entry+'\n'+hook+'\n'):raise ValueError('Unexpected geometry hook')
  body=body.replace(hook+'\n','',1)
 if hashlib.sha256(body.encode()).hexdigest()!=digest:raise ValueError('Original lifted body changed '+va)
 if hook not in source:source=source.replace(entry,entry+'\n'+hook,1)
 anchor='#include "functions.h"';header='#include "geometry_fast.h"'
 if source.count(anchor)!=1:raise ValueError('Unexpected include anchor')
 if header not in source:source=source.replace(anchor,anchor+'\n'+header,1)
 elif source.count(header)!=1:raise ValueError('Duplicate geometry header')
 return source

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--apply',action='store_true');args=parser.parse_args()
 data=(ROOT/'setup_conflict_-_desert_storm_1.0_(21657)/DesertStorm.exe').read_bytes()
 if hashlib.sha256(data).hexdigest()!=SHA:raise SystemExit('Unsupported original PE SHA256')
 pe=struct.unpack_from('<I',data,0x3c)[0];count=struct.unpack_from('<H',data,pe+6)[0];optional=struct.unpack_from('<H',data,pe+20)[0];base=struct.unpack_from('<I',data,pe+52)[0]
 pending=[]
 for va,(unit,digest,entryhex) in SPECS.items():
  rva=int(va,16)-base;entry=None
  for i in range(count):
   section=pe+24+optional+i*40;size,start,rawsize,raw=struct.unpack_from('<IIII',data,section+8)
   if start<=rva and rva+16<=start+rawsize:entry=data[raw+rva-start:raw+rva-start+16].hex();break
  if entry!=entryhex:raise SystemExit('PE entry anchor mismatch '+va)
  path=ROOT/f'build/translated/desertstorm/functions_{unit}.c';source=path.read_text();changed=patched_source(source,va,digest);assert patched_source(changed,va,digest)==changed
  pending.append((path,source,changed))
 for path,source,changed in pending:
  if args.apply and changed!=source:
   temp=path.with_name(f'.{path.name}.{os.getpid()}.tmp')
   try:temp.write_text(changed);temp.replace(path)
   finally:temp.unlink(missing_ok=True)
  print(json.dumps({'unit':str(path.relative_to(ROOT)),'applied':args.apply,'would_change':changed!=source,'enable':'DS_FAST_MATH=1'}))
if __name__=='__main__':main()
