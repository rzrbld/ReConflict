#!/usr/bin/env python3
"""Stage a checked local-variable version of the profiled vector-length helper.
Preserve the original reciprocal-root approximation; this is not a sqrt rewrite.
"""
from pathlib import Path
import hashlib,json,re,subprocess
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/length-scalar'
OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'build/translated/desertstorm/functions_0065.c').read_text()
assert hashlib.sha256(source.encode()).hexdigest()=='bc8ace6bcada2a5c6fa00ec8077db48a9835fd75776baade200a6757042549f5'
archive=ROOT/'build/render-thread-game/snapshot/build/translated/desertstorm/lifted.aarch64.a'
obj=subprocess.check_output(['ar','p',str(archive),'functions_0065.aarch64.o'])
assert hashlib.sha256(obj).hexdigest()=='bf785c8dae3b423814f5feb72f673ae53d53cf7dbd8f2e4528149e9c62403e82'
assert obj==(ROOT/'build/translated/desertstorm/functions_0065.aarch64.o').read_bytes()
f=re.search(r'^void recomp_desertstorm_0051FCC0\(void\) \{.*?(?=^void |\Z)',source,re.M|re.S).group()
digest=hashlib.sha256(f.encode()).hexdigest()
assert digest=='3967f6be7722c201f35f9527333428b26cc8215b1146e96c3ba901453fd24b75'
prefix='#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\n'
(OUT/'reference.c').write_text(prefix+f.replace('recomp_desertstorm_0051FCC0','reference_0051FCC0'))
stack=[f'g_st[{i}]' for i in range(8)];shadow=[f'g_st_i64[{i}]' for i in range(8)]
depth=maximum=count=0;out=[]
def subst(s):return re.sub(r'_st\[(\d)\]',lambda m:stack[int(m[1])],s)
def new(expr):
 global count
 count+=1;v=f'v{count}';out.append(f'    double {v} = {expr};');return v
def pop():
 global depth
 value=stack.pop(0);stack.append('0.0');shadow.pop(0);shadow.append('0');depth-=1
 assert depth>=0
 return value
for line in f.splitlines():
 code=line.split('/*')[0].strip();comment=(' /*'+line.split('/*',1)[1]) if '/*' in line else ''
 assert not any(x in code for x in ('goto ', 'RECOMP_CALL')),code
 if code.startswith('void recomp_'):
  out.append('int ds_length_0051FCC0(void) {\n    if(!eligible())return 0;');continue
 if code.startswith('fp_push('):
  value=new(subst(code[len('fp_push('):-2]));stack=[value]+stack[:7];shadow=['0']+shadow[:7]
  depth+=1;maximum=max(maximum,depth);continue
 if code=='{ _st[0] = _st[0]; g_st_i64[0] = g_st_i64[0]; fp_pop(); }':pop();continue
 if 'fp_pop()' in code:
  assert code.count('fp_pop()')==1
  value=stack[0]
  if code.startswith('{ double _v = fp_pop();'):
   pop();replacement=subst(code.replace('fp_pop()',value))
  else:replacement=subst(code.replace('fp_pop()',value));pop()
  out.append('    '+replacement+comment);continue
 if code.startswith('_st['):
  m=re.fullmatch(r'_st\[(\d)\] (\*=|/=|\+=|-=|=) (.*);',code);assert m,code
  idx=int(m[1]);expr=subst(m[3])
  if m[2]!='=':expr=stack[idx]+' '+m[2][0]+' ('+expr+')'
  stack[idx]=new(expr);continue
 if 'return;' in code:
  assert depth==1 and maximum==5
  assert stack[1:]==[f'g_st[{i}]' for i in range(3)]+['0.0']*4
  assert shadow==['0']+[f'g_st_i64[{i}]' for i in range(3)]+['0']*4
  out.extend(['    /* Shift surviving incoming slots as bytes before writing the result. */',
   '    memmove(g_st+1,g_st,3*sizeof(*g_st));',
   '    memmove(g_st_i64+1,g_st_i64,3*sizeof(*g_st_i64));',
   f'    g_st[0]={stack[0]}; g_st_i64[0]=0;',
   '    memset(g_st+4,0,4*sizeof(*g_st));',
   '    memset(g_st_i64+4,0,4*sizeof(*g_st_i64));',
   '    g_fp_top++;'])
  line=line.replace('return;','return 1;')
 assert not any(x in code for x in ('_st[','fp_pop','fp_push','g_st_i64')),code
 out.append(line)
guard='''#include <limits.h>
#ifdef __FAST_MATH__
#error Vector length requires ordered arithmetic
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("fp-contract=off")
#endif
static int enabled(void) {
 static int initialized,value;
 if(!initialized){const char*s=getenv("DS_FAST_LENGTH");value=s&&!strcmp(s,"1");initialized=1;}
 return value;
}
static int overlaps(uint32_t a,uint32_t n,uint32_t b,uint32_t m) {
 return (uint64_t)a+n>b && (uint64_t)b+m>a;
}
static uint32_t bits_at(uint32_t a) {uint32_t b;memcpy(&b,(void*)ADDR(a),4);return b;}
static int eligible(void) {
 if(!enabled() || g_fp_top>INT_MAX-5)return 0;
 uint32_t s=g_esp;if(s>UINT32_MAX-8)return 0;
 uint32_t src=MEM32(s+4);
 if(!src || src>UINT32_MAX-12 || overlaps(src,12,s,8))return 0;
 if(overlaps(s,8,0x5D8364,4)||overlaps(s,8,0x5D8768,4)||overlaps(s,8,0x5D843C,4))return 0;
 // |coordinate|<2^63 ensures the rounded sum of three squares stays finite.
 // Exceptional values use the original routine to preserve NaN payloads.
 for(unsigned i=0;i<3;i++)if((bits_at(src+4*i)&0x7fffffffu)>=0x5f000000u)return 0;
 if((bits_at(0x5D8364)&0x7f800000u)==0x7f800000u ||
    (bits_at(0x5D8768)&0x7f800000u)==0x7f800000u ||
    (bits_at(0x5D843C)&0x7f800000u)==0x7f800000u)return 0;
 return 1;
}
'''
(OUT/'length_scalar.c').write_text(prefix+guard+'\n'.join(out)+'\n')
(OUT/'length_scalar.h').write_text('int ds_length_0051FCC0(void);\n')
source=source.replace('#include "functions.h"','#include "functions.h"\nint ds_length_0051FCC0(void);',1)
source=source.replace('void recomp_desertstorm_0051FCC0(void) {','void recomp_desertstorm_0051FCC0(void) {\n    if(ds_length_0051FCC0())return; /* checked local x87 scalarization */',1)
(OUT/'functions_0065.c').write_text(source)
(OUT/'functions.h').write_bytes((ROOT/'build/translated/desertstorm/functions.h').read_bytes())
report={'reference_sha256':digest,'scalar_values':count,'peak_depth':maximum,'final_depth_delta':depth}
(OUT/'manifest.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
