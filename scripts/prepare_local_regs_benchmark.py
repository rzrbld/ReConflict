#!/usr/bin/env python3
"""Stage two real hot routines, original setjmp, and ABI fixtures only."""
from pathlib import Path
import re
from stage_local_regs import functions,transform
ROOT=Path(__file__).resolve().parents[1];out=ROOT/'build/local-regs-stage/benchmark';out.mkdir(parents=True,exist_ok=True)
want={'recomp_desertstorm_00540470','recomp_desertstorm_005405A0','recomp_desertstorm_005C9654'};bodies=[]
for number in ('0072','0114'):
 source=(ROOT/f'build/translated/desertstorm/functions_{number}.c').read_text()
 for name,start,end,*_ in functions(source):
  if name in want:bodies.append(source[start:end]);want.remove(name)
if want:raise SystemExit('Missing '+str(want))
source='\n'.join(bodies)+'\n'+(ROOT/'tests/local_regs_fixtures.c').read_text()
local,report=transform(source,extra_hooks={'ds_test_native_hook'})
names=[row['name']for row in report];preamble='#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\n#include "cpu_features.h"\n#include "nonlocal.h"\nint ds_math_fast_540470(void),ds_math_fast_5405a0(void),ds_test_native_hook(void);\nvoid native_consumer(void),fixture_throw(void);\n'
for variant,text in [('global',source),('local',local)]:
 mapping={name:variant+'_'+name.removeprefix('recomp_') for name in names}
 text=re.sub(r'\b(?:'+ '|'.join(map(re.escape,names))+r')\b',lambda m:mapping[m.group()],text)
 (out/(variant+'.c')).write_text(preamble+text+'\n')
import json
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print('Staged',len(names),'functions:',out)
