from pathlib import Path
import re,hashlib
root=Path(__file__).resolve().parents[1];stage=root/'build/jinvmjt-tests';stage.mkdir(parents=True,exist_ok=True)
assert hashlib.sha256((root/'setup_conflict_-_desert_storm_1.0_(21657)/DesertStorm.exe').read_bytes()).hexdigest()=='d712f5ec82c14da66b098332b3cc9c99689091f46317112ea35dfd393dc3e164'
s=(root/'build/translated/desertstorm/functions_0097.c').read_text();f=re.search(r'^void recomp_desertstorm_00595270\(void\) \{.*?(?=^void |\Z)',s,re.M|re.S).group();f=re.sub(r'^    if\(ds_jinvmjt_fast_kernel\([01]\)\)\{RECOMP_FLAGS_IN\(\);goto L_0059541E;\}\n','',f,flags=re.M)
assert hashlib.sha256(f.encode()).hexdigest()=='5634df712c10db0cbd4b3af465f0a1ba5e0fa126e151a8988284b12f6801f1c6'
print('Original595270 textSHA',hashlib.sha256(f.encode()).hexdigest())
prefix='#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\n#include "jinvmjt_fast.h"\n'
block=''
for mode,start,end in [(1,'00595361','005953C3'),(0,'005953D3','0059541E')]:
 chunk=f[f.index('L_'+start+':'):f.index('L_'+end+':')]
 block+=f'void reference_block_{mode}(void){{int _fpu_cmp=0;uint32_t _cf=0,_flag_a=0,_flag_b=0,_flag_k=FK_NONE;RECOMP_FLAGS_IN();\n'+chunk+'RECOMP_FLAGS_OUT();}\n'
(stage/'blocks-reference.c').write_text(prefix+block)
protos='void recomp_desertstorm_00599970(void),recomp_desertstorm_00599AB0(void);\nextern unsigned ds_accepted[2],ds_declined;\n'
(stage/'full-reference.c').write_text(prefix+protos+f.replace('recomp_desertstorm_00595270','reference_00595270'))
for label,mode in [('00595361',1),('005953D3',0)]:f=f.replace('L_'+label+':\n','L_'+label+':\n    if(ds_jinvmjt_fast_kernel('+str(mode)+')){++ds_accepted['+str(mode)+'];RECOMP_FLAGS_IN();goto L_0059541E;}++ds_declined;\n',1)
(stage/'full-native.c').write_text(prefix+protos+f.replace('recomp_desertstorm_00595270','native_00595270'))
# Original profiling-disabled callees retained; unsupported enabled paths abort.
ps=(root/'build/translated/desertstorm/functions_0098.c').read_text()
def ex(va):return re.search(r'^void recomp_desertstorm_'+va+r'\(void\) \{.*?(?=^void |\Z)',ps,re.M|re.S).group()
profiles=[ex('00599970'),ex('00599AB0')]
for body,digest in zip(profiles,['b0ba916780f2faa10d74efaf7a8618c80a9c20654c66f0570fc37dc3a3c2343e','adbbcf1d41c3406859dd6366c5f814cc643701b4f1132d054e07dd69a00c155b']):assert hashlib.sha256(body.encode()).hexdigest()==digest
body='\n'.join(profiles);deps=set(re.findall(r'RECOMP_CALL\((recomp_desertstorm_[0-9A-F]+)\)',body))-{'recomp_desertstorm_00599970','recomp_desertstorm_00599AB0'}
stub='\n'.join('void '+dep+'(void){fprintf(stderr,"Unexpected enabled-profile dependency '+dep+'\\n");abort();}' for dep in sorted(deps))
(stage/'precise-callees.c').write_text(prefix+stub+'\nvoid recomp_desertstorm_00599AB0(void);\n'+body)
