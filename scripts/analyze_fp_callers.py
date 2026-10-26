#!/usr/bin/env python3
"""Attribute samples inside exact FPU helpers to their immediate native callers."""
import argparse,bisect,collections,csv,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('samples',type=Path);p.add_argument('maps',type=Path);p.add_argument('elf',type=Path);p.add_argument('--before',type=Path);p.add_argument('--output',type=Path);a=p.parse_args()
def counts(path):return {(int(x['host_pc'],16),int(x['host_lr'],16)):int(x['cpu_samples']) for x in csv.DictReader(path.open())}
after=counts(a.samples);before=counts(a.before) if a.before else {}
if any(after.get(k,0)<v for k,v in before.items()):raise SystemExit('Invalid cumulative sample interval')
regions=[]
for line in a.maps.read_text().splitlines():
 f=line.split()
 if len(f)>=6:
  lo,hi=(int(x,16) for x in f[0].split('-'));regions.append((lo,hi,int(f[2],16),f[-1]))
syms={}
for line in subprocess.check_output(['readelf','-sW',a.elf],text=True).splitlines():
 f=line.split()
 if len(f)>=8 and f[3]=='FUNC' and f[6]!='UND':
  va=int(f[1],16);size=int(f[2],16 if f[2].startswith('0x') else 10)
  if size and (va not in syms or size>syms[va][0]):syms[va]=(size,f[7])
starts=sorted(syms)
def resolve(pc):
 for lo,hi,offset,name in regions:
  if lo<=pc<hi:
   rel=pc-lo+offset
   if name.endswith('desert-storm.aarch64'):
    i=bisect.bisect_right(starts,rel)-1
    if i>=0 and rel<starts[i]+syms[starts[i]][0]:return syms[starts[i]][1],rel
   return name+'+'+hex(rel),rel
 return '<unmapped>',pc
callers=collections.Counter();sites=collections.Counter();total=0
for key,amount in after.items():
 n=amount-before.get(key,0);total+=n
 if n and resolve(key[0])[0] in ('ds_fp_stack_push_exact','ds_fp_stack_pop_exact'):
  caller,pc=resolve(key[1]-4);callers[caller]+=n;sites[f'{caller}+callsite:{pc:#x}']+=n
out={'total_interval_samples':total,'fpu_helper_samples':sum(callers.values()),'immediate_callers':callers.most_common(),'callsites':sites.most_common(100),'limitation':'PC/LR samples identify immediate FPU callers, not full AI/animation call stacks.'}
s=json.dumps(out,indent=2)+'\n'
if a.output:a.output.write_text(s)
print(s)
