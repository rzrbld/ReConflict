#!/usr/bin/env python3
"""Scalarize only the straight-line body of three checked rotation routines.
Original arithmetic, memory access order, rounding stores and branch stay intact.
The normal generated translation and frozen archives are never edited.
"""
from pathlib import Path
import hashlib, re, json, subprocess
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/rotation-scalar'
OUT.mkdir(parents=True,exist_ok=True)
source_path=ROOT/'build/translated/desertstorm/functions_0071.c'
source=source_path.read_text()
# Verify this generated unit's recorded object is exactly the frozen member.
archive=ROOT/'build/render-thread-game/snapshot/build/translated/desertstorm/lifted.aarch64.a'
frozen_object=subprocess.check_output(['ar','p',str(archive),'functions_0071.aarch64.o'])
assert hashlib.sha256(frozen_object).hexdigest()=='9acb63211a686f860f6a139da043e9137c2a39be25394fef1c64d4069730a216'
assert frozen_object==(ROOT/'build/translated/desertstorm/functions_0071.aarch64.o').read_bytes()
(OUT/'functions.h').write_bytes((ROOT/'build/translated/desertstorm/functions.h').read_bytes())
(OUT/'input-sha256.txt').write_text(hashlib.sha256(source.encode()).hexdigest()+'  '+str(source_path.relative_to(ROOT))+'\n')
checks={
'0053F9F0':('f8a0849c6ee97dd00c956247890a492cb5077b544ac17dfbd9b3ea6661344036','0053FA08','0053FAC2'),
'0053FAD0':('1f48bc7a2bcd3cea9cab387c2c5abfd7512033ed93e4bc37b153f0c04dccd4ce','0053FAE8','0053FB9F'),
'0053FBB0':('a837b59804ef2d78b331a07adaf3590f5f9ac31d7d08a5bae748d73b742b56dd','0053FBC8','0053FC7F')}
prefix='#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\n'
fast=[prefix,'''#include <limits.h>
#ifdef __FAST_MATH__
#error Rotation scalarization requires ordered arithmetic
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("fp-contract=off")
#endif
static int enabled(void) {
 static int initialized,value;
 if(!initialized){const char*s=getenv("DS_FAST_ROTATION");value=s&&!strcmp(s,"1");initialized=1;}
 return value;
}
static int overlaps(uint32_t a,uint32_t n,uint32_t b,uint32_t m) {
 return (uint64_t)a+n>b && (uint64_t)b+m>a;
}
static int finite_at(uint32_t a) {
 uint32_t bits;memcpy(&bits,(void*)ADDR(a),4);
 return (bits&0x7f800000u)!=0x7f800000u;
}
static int eligible(void) {
 if(!enabled() || g_fp_top>INT_MAX-5)return 0;
 uint32_t s=g_esp,m=g_ecx;
 if(s<8 || s>UINT32_MAX-8 || !m || m>UINT32_MAX-68)return 0;
 if(overlaps(m,68,s-8,16) || overlaps(m,68,0x5D8344,4) ||
    overlaps(m,68,0x5D8454,8) || overlaps(s-8,16,0x5D8344,4) ||
    overlaps(s-8,16,0x5D8454,8))return 0;
 if(!finite_at(s+4) || !finite_at(0x5D8344) ||
    !finite_at(0x5D8454) || !finite_at(0x5D8458))return 0;
 if(*(float*)ADDR(s+4)==*(float*)ADDR(0x5D8344))return 0;
 for(unsigned i=0;i<16;i++)if(!finite_at(m+4*i))return 0;
 return 1;
}
''']
reference=[prefix];report={}
for addr,(digest,begin,end) in checks.items():
    f=re.search(r'^void recomp_desertstorm_'+addr+r'\(void\) \{.*?(?=^void |\Z)',source,re.M|re.S).group()
    assert hashlib.sha256(f.encode()).hexdigest()==digest,addr
    reference.append(f.replace('recomp_desertstorm_'+addr,'reference_'+addr))
    a=f.index('L_'+begin+':')+len('L_'+begin+':')
    b=f.index('L_'+end+':')
    body=f[a:b]
    stack=[f'g_st[{i}]' for i in range(8)]
    shadow=[f'g_st_i64[{i}]' for i in range(8)]
    depth=maximum=count=0
    lines=[]
    def subst(s):return re.sub(r'_st\[(\d)\]',lambda m:stack[int(m[1])],s)
    def pop():
        global depth
        v=stack.pop(0);stack.append('0.0');shadow.pop(0);shadow.append('0');depth-=1
        assert depth>=0
        return v
    def new(expr):
        global count
        count+=1;v=f'r{count}';lines.append(f'    double {v} = {expr};');return v
    for line in body.splitlines():
        code=line.split('/*')[0].strip()
        comment=(' /*'+line.split('/*',1)[1]) if '/*' in line else ''
        if not code:continue
        assert not any(x in code for x in ('goto ', 'RECOMP_CALL','return','if(', 'if (')),code
        if code.startswith('fp_push('):
            value=new(subst(code[len('fp_push('):-2]));stack=[value]+stack[:7];shadow=['0']+shadow[:7]
            depth+=1;maximum=max(maximum,depth);continue
        if code.startswith('fp_xch('):
            idx=int(code[len('fp_xch('):-2]);stack[0],stack[idx]=stack[idx],stack[0];shadow[0],shadow[idx]=shadow[idx],shadow[0];continue
        if 'fp_pop()' in code:
            assert code.count('fp_pop()')==1
            value=stack[0]
            if code.startswith('{ double _v = fp_pop();'):
                pop();replacement=subst(code.replace('fp_pop()',value))
            else:
                replacement=subst(code.replace('fp_pop()',value));pop()
            lines.append('    '+replacement+comment);continue
        if code.startswith('_st['):
            m=re.fullmatch(r'_st\[(\d)\] (\*=|/=|\+=|-=|=) (.*);(.*)',code);assert m,code
            idx=int(m[1]);expr=subst(m[3])
            if m[2]!='=':expr=stack[idx]+' '+m[2][0]+' ('+expr+')'
            # sin/cos lines also update the local comparison state.
            if ';' in expr:
                expr,tail=expr.split(';',1);tail=tail.strip()+';'
            else:tail=''
            stack[idx]=new(expr)
            if tail:lines.append('    '+tail)
            continue
        assert not any(x in code for x in ('fp_', '_st[', 'g_st')),code
        lines.append(line)
    assert depth==0 and maximum==5,(addr,depth,maximum)
    assert stack==[f'g_st[{i}]' for i in range(3)]+['0.0']*5
    assert shadow==[f'g_st_i64[{i}]' for i in range(3)]+['0']*5
    lines.append('    /* Preserve the fixed-window stack truncation, including inactive slots. */')
    lines.append('    memset(g_st+3,0,5*sizeof(*g_st));')
    lines.append('    memset(g_st_i64+3,0,5*sizeof(*g_st_i64));')
    optimized=f[:a]+'\n'+'\n'.join(lines)+'\n\n'+f[b:]
    optimized=optimized.replace('void recomp_desertstorm_'+addr+'(void) {',
        'int ds_rotation_'+addr+'(void) {\n    if(!eligible())return 0;')
    optimized=optimized.replace('return;', 'return 1;')
    fast.append(optimized)
    hook='    if(ds_rotation_'+addr+'())return; /* checked local x87 scalarization */\n'
    source=source.replace('void recomp_desertstorm_'+addr+'(void) {\n',
        'void recomp_desertstorm_'+addr+'(void) {\n'+hook)
    report[addr]={'reference_sha256':digest,'scalar_values':count,'peak_stack_depth':maximum}
prototypes='\n'.join('int ds_rotation_'+a+'(void);' for a in checks)+'\n'
source=source.replace('#include "quaternion_fast.h"','#include "quaternion_fast.h"\n'+prototypes)
(OUT/'functions_0071.c').write_text(source)
(OUT/'rotation_scalar.c').write_text('\n'.join(fast))
(OUT/'reference.c').write_text('\n'.join(reference))
(OUT/'rotation_scalar.h').write_text(prototypes)
(OUT/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
