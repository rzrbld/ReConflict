#!/usr/bin/env python3
"""Resolve sampled native PCs against an explicitly matching ELF and /proc maps.
An optional earlier snapshot yields interval counts. Symbols are evidence of
where samples landed, not caller attribution or measured wall-clock duration.
"""
import argparse,bisect,collections,csv,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('samples',type=Path);p.add_argument('maps',type=Path)
p.add_argument('--before',type=Path);p.add_argument('--module',action='append',default=[],help='remote-basename=local-ELF')
p.add_argument('--output',type=Path);a=p.parse_args()
def counts(path):
 return {int(r['host_pc'],16):int(r['cpu_samples'])for r in csv.DictReader(path.open())}
current=counts(a.samples);before=counts(a.before)if a.before else{}
if any(current.get(pc,0)<n for pc,n in before.items()):raise SystemExit('Snapshots do not form a cumulative interval')
regions=[]
for line in a.maps.read_text().splitlines():
 fields=line.split()
 if len(fields)<6:continue
 low,high=(int(v,16)for v in fields[0].split('-'))
 regions.append((low,high,int(fields[2],16),fields[-1]))
tables={}
for entry in a.module:
 name,path=entry.split('=',1);symbols={}
 for line in subprocess.check_output(['readelf','-sW',path],text=True).splitlines():
  f=line.split()
  if len(f)>=8 and f[3]in('FUNC','IFUNC')and f[6]!='UND':
   try:addr,size=int(f[1],16),int(f[2],16 if f[2].startswith("0x") else 10)
   except ValueError:continue
   if size and (addr not in symbols or size>symbols[addr][0]):symbols[addr]=(size,f[7])
 starts=sorted(symbols);tables[name]=(starts,symbols)
modules=collections.Counter();functions=collections.Counter();locations=collections.Counter();unknown=collections.Counter()
for pc,total in current.items():
 n=total-before.get(pc,0)
 if not n:continue
 for low,high,offset,name in regions:
  if low<=pc<high:
   modules[name]+=n;relative=pc-low+offset;table=tables.get(Path(name).name)
   if table:
    starts,syms=table;i=bisect.bisect_right(starts,relative)-1
    if i>=0 and relative<starts[i]+syms[starts[i]][0]:
     functions[name+'!'+syms[starts[i]][1]]+=n
     locations[name+'!'+syms[starts[i]][1]+f'@0x{starts[i]:x}']+=n
    else:unknown[name+f'+0x{relative:x}']+=n
   break
 else:modules['<unmapped>']+=n
report={'total_interval_samples':sum(modules.values()),'modules':modules.most_common(), 'functions':functions.most_common(60),'function_locations':locations.most_common(60),'function_name_note':'functions aggregates equal symbol names; function_locations distinguishes local symbols by ELF address','unresolved_pcs':unknown.most_common(40)}
out=json.dumps(report,indent=2)+'\n'
if a.output:a.output.write_text(out)
print(out)
