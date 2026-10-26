from pathlib import Path
import re,hashlib
root=Path(__file__).resolve().parents[1];stage=root/'build/culling-prefix-tests';stage.mkdir(parents=True,exist_ok=True)
assert hashlib.sha256((root/'setup_conflict_-_desert_storm_1.0_(21657)/DesertStorm.exe').read_bytes()).hexdigest()=='d712f5ec82c14da66b098332b3cc9c99689091f46317112ea35dfd393dc3e164'
s=(root/'build/translated/desertstorm/functions_0061.c').read_text();f=re.search(r'^void recomp_desertstorm_0050C350\(void\) \{.*?(?=^void |\Z)',s,re.M|re.S).group();f=re.sub(r'^    if\(ds_culling_fast_50c350\(\)\)return;.*\n','',f,flags=re.M)
f=re.sub(r'^    \{int path=ds_culling_prefix_50c359\(\);if\(path\)\{RECOMP_FLAGS_IN\(\);if\(path==1\)goto L_0050C40F;goto L_0050C48A;\}\}\n','',f,flags=re.M)
assert hashlib.sha256(f.encode()).hexdigest()=='b3efaf7af04e903deb758d4054ce73d7c45b3e8603c9872618d592a6cfd384f3'
print('original unhooked body SHA256',hashlib.sha256(f.encode()).hexdigest())
hook='''L_0050C359:
    {int path=ds_culling_prefix_50c359();if(path){RECOMP_FLAGS_IN();if(path==1)goto L_0050C40F;goto L_0050C48A;}}
'''
for mode in ['full','prefix']:
 b=f
 if mode=='prefix':b=b[:b.index('L_0050C40F:')]+'L_0050C40F:\n    RECOMP_FLAGS_OUT();return;\nL_0050C48A:\n    RECOMP_FLAGS_OUT();return;\n}\n'
 reference=b.replace('recomp_desertstorm_0050C350','reference_0050C350');candidate=b.replace('L_0050C359:\n',hook).replace('recomp_desertstorm_0050C350','candidate_0050C350')
 (stage/f'{mode}.c').write_text('#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\n#include "culling_fast.h"\n'+reference+'\n'+candidate)
