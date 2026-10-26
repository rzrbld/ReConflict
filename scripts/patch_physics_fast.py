#!/usr/bin/env python3
"""Review/apply bounded inline rank update to both original factorize entries."""
import argparse,json,os
from physics_fast_spec import ROOT,validate_binary,extract,original_body,FUNCTION_SHA,ANCHOR,HOOK,HEADER

def patched_source(source):
 for va in FUNCTION_SHA:
  body=extract(source,va);original=original_body(body,va)
  if original.count(ANCHOR)!=1:raise ValueError('Expected sole rank-update label '+va)
  replacement=original.replace(ANCHOR,ANCHOR+HOOK,1);source=source.replace(body,replacement,1)
 anchor='#include "functions.h"'
 if source.count(anchor)!=1:raise ValueError('Unexpected include anchor')
 if HEADER not in source:source=source.replace(anchor,anchor+'\n'+HEADER,1)
 elif source.count(HEADER)!=1:raise ValueError('Duplicate physics header')
 return source

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--apply',action='store_true');args=parser.parse_args();validate_binary()
 path=ROOT/'build/translated/desertstorm/functions_0097.c';source=path.read_text();changed=patched_source(source)
 assert patched_source(changed)==changed
 if args.apply and changed!=source:
  temporary=path.with_name(f'.{path.name}.{os.getpid()}.tmp')
  try:temporary.write_text(changed);temporary.replace(path)
  finally:temporary.unlink(missing_ok=True)
 print(json.dumps({'unit':str(path.relative_to(ROOT)),'applied':args.apply,'would_change':changed!=source,'entries':list(FUNCTION_SHA),'enable':'DS_FAST_MATH=1','fallback':'Original rank-update region, including original local/global flag separation'}))
if __name__=='__main__':main()
