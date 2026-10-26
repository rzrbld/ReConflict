#!/usr/bin/env python3
"""Review/apply optional immutable-main cache after original dynamic lookups."""
import argparse,hashlib,json,os,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DIGEST='967ef35319e6a0c11093a89945d28ac1059efa2de1b77b1e7093ff7b85060669'
HOOK='''    /* Optional immutable-main cache; keep recovered/live-module precedence. */
    extern int ds_dispatch_cache_active __attribute__((weak));
    extern recomp_func_t ds_dispatch_main_lookup(uint32_t) __attribute__((weak));
    if(&ds_dispatch_cache_active && ds_dispatch_cache_active && ds_dispatch_main_lookup)
        return ds_dispatch_main_lookup(va);
'''
def patched_source(source):
 found=re.search(r'^recomp_func_t recomp_lookup\(uint32_t va\) \{.*?(?=^recomp_func_t recomp_lookup_manual)',source,re.M|re.S)
 if not found:raise ValueError('Missing host dispatch function')
 body=found.group();original=body
 if HOOK in body:
  if body.count(HOOK)!=1:raise ValueError('Duplicate cache hook')
  original=body.replace(HOOK,'',1)
 if hashlib.sha256(original.encode()).hexdigest()!=DIGEST:raise ValueError('Original lookup changed; re-review precedence')
 anchor='    uint32_t lo = 0, hi = recomp_dispatch_count;'
 if original.count(anchor)!=1:raise ValueError('Unexpected search anchor')
 changed=original.replace(anchor,HOOK+anchor,1)
 return source[:found.start()]+changed+source[found.end():]
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--apply',action='store_true');args=parser.parse_args()
 path=ROOT/'third_party/pcrecomp/runtime/win32hle/host_lite.c';source=path.read_text();changed=patched_source(source);assert patched_source(changed)==changed
 if args.apply and source!=changed:
  temporary=path.with_name(f'.{path.name}.{os.getpid()}.tmp')
  try:temporary.write_text(changed);temporary.replace(path)
  finally:temporary.unlink(missing_ok=True)
 print(json.dumps({'file':str(path.relative_to(ROOT)),'applied':args.apply,'would_change':source!=changed,'default':'disabled','size':1024}))
if __name__=='__main__':main()
