#!/usr/bin/env python3
from pathlib import Path
import re,hashlib
from physics_fast_spec import validate_binary,original_body,HOOK
root=Path(__file__).resolve().parents[1];stage=root/'build/physics-stage';stage.mkdir(parents=True,exist_ok=True);validate_binary()
s=(root/'build/translated/desertstorm/functions_0097.c').read_text()
prefix='#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\n'
def extract(s,va):return re.search(r'^void recomp_desertstorm_'+va+r'\(void\) \{.*?(?=^void |\Z)',s,re.M|re.S).group()
refs=[];fast=[]
for va,digest in [('00595B10','d37394fe0b5e870dc622fe76c139b84d3439accea4702c3c4e6f9d2ed92c2a67'),('00595B59','3a65a512c048766ede91ad027efec3c362d002c14a8affe2801f7c10f8086f20')]:
 f=original_body(extract(s,va),va);assert hashlib.sha256(f.encode()).hexdigest()==digest
 if va=='00595B10':f=f.replace('L_00595B59:\n','L_00595B59:\n    if(ds_capture){ds_capture=0;return;}\n')
 refs.append(f.replace('recomp_desertstorm_'+va,'reference_'+va))
 insertion='L_00595BC5:\n'+HOOK.replace('if(ds_physics_rank_update_eligible()) {','if(ds_physics_rank_update_eligible()) { ++ds_accepted;')+'    ++ds_declined;\n'
 fast.append(f.replace('L_00595BC5:\n',insertion,1).replace('recomp_desertstorm_'+va,'native_'+va))
prototypes='void recomp_desertstorm_00599970(void),recomp_desertstorm_00599AB0(void),recomp_desertstorm_00567430(void);\nextern int ds_capture;extern unsigned ds_accepted,ds_declined;\n'
(stage/'full-reference.c').write_text(prefix+prototypes+'\n'.join(refs));(stage/'full-native.c').write_text(prefix+'#include \"physics_fast.h\"\n'+prototypes+'\n'.join(fast))
# Use actual guest implementations; fail closed if unsupported profiling-enabled dependencies are reached.
ps=(root/'build/translated/desertstorm/functions_0098.c').read_text();alloc=extract((root/'build/translated/desertstorm/functions_0081.c').read_text(),'00567430')
profiles=[extract(ps,va) for va in ['00599970','00599AB0']]
for body,digest in zip(profiles+[alloc],['b0ba916780f2faa10d74efaf7a8618c80a9c20654c66f0570fc37dc3a3c2343e','adbbcf1d41c3406859dd6366c5f814cc643701b4f1132d054e07dd69a00c155b','029fdd78be77dc5c3df1a62ebee4fd312a33b699ea2480911748ebc5b2b123fa']):assert hashlib.sha256(body.encode()).hexdigest()==digest
allbody='\n'.join(profiles+[alloc]);deps=set(re.findall(r'RECOMP_CALL\((recomp_desertstorm_[0-9A-F]+)\)',allbody))-{'recomp_desertstorm_00599970','recomp_desertstorm_00599AB0','recomp_desertstorm_00567430'}
# Indirect profiling paths remain compiled; test dispatch aborts if reached.
stub='\n'.join('void '+dep+'(void){fprintf(stderr,"Unexpected enabled-profile dependency '+dep+'\\n");abort();}' for dep in sorted(deps))
(stage/'precise-callees.c').write_text(prefix+stub+'\nvoid recomp_desertstorm_00599AB0(void);\n'+allbody)
print('Full native/reference emitted; fail-closed profiling deps',sorted(deps))
