#!/usr/bin/env python3
"""Isolate a profiled leaf routine to test exact FP-helper inlining first."""
from pathlib import Path
import hashlib,json,re
root=Path(__file__).resolve().parents[1]
out=root/'build/fp-inline';out.mkdir(parents=True,exist_ok=True)
source=(root/'build/translated/desertstorm/functions_0075.c').read_text()
assert hashlib.sha256(source.encode()).hexdigest()=='0906aa6093d96b730f423ac0cbb0e77d07e42f4a76be8cc0105abd93535bac6e'
f=re.search(r'^void recomp_desertstorm_00551A40\(void\).*?(?=^void |\Z)',source,re.M|re.S).group()
assert 'RECOMP_CALL' not in f
prefix='#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\n'
(out/'reference.c').write_text(prefix+f.replace('recomp_desertstorm_00551A40','reference_00551A40'))
guard=r"""
static int ds_plane_inline_eligible(void) {
    const uint32_t c=g_ecx,s=g_esp;
    if(c>UINT32_MAX-16u || s>UINT32_MAX-12u || s<32u)return 0;
    const int32_t n=(int32_t)MEM32(c);
    if(n<=0)return 1;
    if(n>32 || c>UINT32_MAX-16u-(uint32_t)n*16u)return 0;
    const uint32_t sphere=MEM32(s+4),out=MEM32(s+8);
    if(sphere>UINT32_MAX-16u)return 0;
    // A write to the stack, count or source header can change later pointers
    // or loop bounds. Keep those unusual aliases on the original path.
    if(out && ((uint64_t)out+4>s-32 && out<(uint64_t)s+12))return 0;
    if(out && ((uint64_t)out+4>c && out<(uint64_t)c+16))return 0;
    if(out && ((uint64_t)out+4>0x5D8C44 && out<0x5D8C48))return 0;
    if(out && ((uint64_t)out+4>0x5D99E0 && out<0x5D99E4))return 0;
    if((uint64_t)c+16+(uint32_t)n*16u>s-32 && c<(uint64_t)s+12)return 0;
    if((uint64_t)sphere+16>s-32 && sphere<(uint64_t)s+12)return 0;
    uint32_t bits;
    for(int i=0;i<n*4;i++){memcpy(&bits,(void*)ADDR(c+16+4*i),4);if((bits&0x7f800000u)==0x7f800000u)return 0;}
    for(unsigned i=0;i<4;i++){memcpy(&bits,(void*)ADDR(sphere+4*i),4);if((bits&0x7f800000u)==0x7f800000u)return 0;}
    if((MEM32(0x5D99E0)&0x7f800000u)==0x7f800000u || (MEM32(0x5D8C44)&0x7f800000u)==0x7f800000u)return 0;
    return 1;
}
"""
body=f.replace('recomp_desertstorm_00551A40','candidate_00551A40')
(out/'candidate-unguarded.c').write_text(prefix+'#include "fp_stack_inline.h"\n'+body)
body=body.replace('void candidate_00551A40(void) {','void candidate_00551A40(void) {\n    if(!ds_plane_inline_eligible()){reference_00551A40();return;}')
(out/'candidate-guarded-global.c').write_text(prefix+'#include "fp_stack_inline.h"\nvoid reference_00551A40(void);\n'+guard+body)
local_init='''
    double ds_local_st[8];int64_t ds_local_shadow[8];int ds_local_top=g_fp_top;
    int64_t *ds_global_shadow=g_st_i64;
    memcpy(ds_local_st,g_st,64);memcpy(ds_local_shadow,g_st_i64,64);
#undef _st
#undef _fp_top
#define _st ds_local_st
#define _fp_top ds_local_top
#define g_st_i64 ds_local_shadow
'''
body=body.replace('    int _fpu_cmp = 0;',local_init+'    int _fpu_cmp = 0;',1)
body=body.replace('RECOMP_FLAGS_OUT();','memcpy(g_st,ds_local_st,64);memcpy(ds_global_shadow,ds_local_shadow,64);g_fp_top=ds_local_top; RECOMP_FLAGS_OUT();')
(out/'candidate.c').write_text(prefix+'#include "fp_stack_inline.h"\nvoid reference_00551A40(void);\n'+guard+body)
(out/'fp_stack_inline.h').write_bytes((root/'probe/fp_stack_inline.h').read_bytes())
report={'source_sha256':hashlib.sha256(source.encode()).hexdigest(),'function_sha256':hashlib.sha256(f.encode()).hexdigest(),'function':'00551A40','header_sha256':hashlib.sha256((out/'fp_stack_inline.h').read_bytes()).hexdigest()}
(out/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
