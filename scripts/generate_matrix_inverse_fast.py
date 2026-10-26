#!/usr/bin/env python3
"""Checked straight-line x87 stack scalarization for isolated original inverse."""
from pathlib import Path
import re,hashlib,argparse
root=Path(__file__).resolve().parents[1]
(root/'build/matrix-inverse-tests').mkdir(parents=True,exist_ok=True)
assert hashlib.sha256((root/'setup_conflict_-_desert_storm_1.0_(21657)/DesertStorm.exe').read_bytes()).hexdigest()=='d712f5ec82c14da66b098332b3cc9c99689091f46317112ea35dfd393dc3e164'
s=(root/'build/translated/desertstorm/functions_0038.c').read_text()
f=re.search(r'^void recomp_desertstorm_004A82E6\(void\) \{.*?(?=^void |\Z)',s,re.M|re.S).group()
f=re.sub(r'^    if\(ds_matrix_inverse_fast_4a82e6\(\)\)return;.*\n','',f,flags=re.M)
assert hashlib.sha256(f.encode()).hexdigest()=='c03bdf3f1ee48d3e814d12c63cf4e450cc4d3c742df0357a2304e3e0aae7012f'
print('reference_function_sha256',hashlib.sha256(f.encode()).hexdigest())
(root/'build/matrix-inverse-tests/reference.c').write_text('#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\nvoid recomp_desertstorm_00568210(void);\n'+f.replace('recomp_desertstorm_004A82E6','reference_004A82E6'))
s=(root/'build/translated/desertstorm/functions_0081.c').read_text();finite=re.search(r'^void recomp_desertstorm_00568210\(void\) \{.*?(?=^void |\Z)',s,re.M|re.S).group()
(root/'build/matrix-inverse-tests/finite-reference.c').write_text('#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\n'+finite)
stack=[f'g_st[{i}]' for i in range(8)];shadow=[f'g_st_i64[{i}]' for i in range(8)];top=0;maximum=0;counter=0;out=[];branch=None

def subst(s):return re.sub(r'_st\[(\d)\]',lambda m:stack[int(m[1])],s)
def pop():
 global top
 value=stack.pop(0);stack.append('0.0');shadow.pop(0);shadow.append('0');top-=1;return value
def new(expr):
 global counter
 counter+=1;name=f'v{counter}';out.append(f'    double {name} = {expr};');return name

for l in f.splitlines():
 code=l.split('/*')[0].strip();comment=(' /*'+l.split('/*',1)[1]) if '/*' in l else ''
 if code.startswith('void recomp_'):out.append('int ds_matrix_inverse_fast_4a82e6(void) {\n    if(!eligible())return 0;');continue
 if 'RECOMP_CALL(recomp_desertstorm_00568210)' in code:
  out.append('    PUSH32(esp, RECOMP_RETADDR);')
  body=finite[finite.index('L_00568210:')+len('L_00568210:'):finite.rindex('}')]
  body=body.replace('RECOMP_FLAGS_OUT(); esp += 4; return;','esp += 4;')
  out.append(body);continue
 if 'RECOMP_FLAGS_IN();'==code:continue # already inline finite state
 if code.startswith('fp_push('):
  expr=subst(code[len('fp_push('):-2]);v=new(expr);stack=[v]+stack[:7];shadow=['0']+shadow[:7];top+=1;maximum=max(maximum,top);continue
 if re.fullmatch(r'\{ _st\[0\] = _st\[0\]; g_st_i64\[0\] = g_st_i64\[0\]; fp_pop\(\); \}',code):pop();continue
 if 'fp_pop()' in code:
  value=stack[0]
  if code.startswith('{ double _v = fp_pop();'):
   expression=subst(code.replace('fp_pop()',value));pop()
   # remaining _st[0] is AFTER pop
   expression=code.replace('fp_pop()',value);expression=subst(expression)
  else:expression=subst(code.replace('fp_pop()',value));pop()
  out.append('    '+expression+comment);continue
 if code.startswith('_st['):
  m=re.match(r'_st\[(\d)\] (\*=|/=|\+=|-=|=) (.*);',code);assert m,code
  idx=int(m[1]);expr=subst(m[3]);old=stack[idx]
  if m[2]!='=':expr=old+' '+m[2][0]+' ('+expr+')'
  stack[idx]=new(expr);continue
 if 'goto L_004A866F' in code:branch=(stack[:],shadow[:],top)
 if code=='L_004A866F:':
  assert branch==(stack,shadow,top),(branch,stack,shadow,top)
  out.append(l)
  for i in range(8):out.append(f'    g_st[{i}]={stack[i]}; g_st_i64[{i}]={shadow[i]};')
  if top:out.append(f'    g_fp_top+={top};')
  continue
 if '_st[' in code:l='    '+subst(code)+comment
 if 'return;' in l:l=l.replace('return;','return 1;')
 out.append(l)
assert maximum<=8,maximum
prefix='''#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
#include "matrix_inverse_fast.h"
#include <limits.h>
#ifdef __FAST_MATH__
#error Inverse requires ordered arithmetic
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("fp-contract=off")
#endif
static int enabled(void){static int init,value;if(!init){const char*s=getenv("DS_FAST_MATH");value=s&&!strcmp(s,"1");init=1;}return value;}
static int overlaps(uint32_t a,uint32_t n,uint32_t b,uint32_t m){return (uint64_t)a+n>b&&(uint64_t)b+m>a;}
static int eligible(void){
 if(!enabled())return 0;
 uint32_t s=g_esp;if(s<100||s>UINT32_MAX-16||g_fp_top>INT_MAX-8)return 0;
 uint32_t dst=MEM32(s+4),det=MEM32(s+8),src=MEM32(s+12);
 if(!dst||!src||dst>UINT32_MAX-64||src>UINT32_MAX-64||(det&&det>UINT32_MAX-4))return 0;
 if(overlaps(src,64,s-100,116)||overlaps(dst,64,s-100,116)||(det&&overlaps(det,4,s-100,116)))return 0;
 for(unsigned i=0;i<16;i++){float v;memcpy(&v,(void*)ADDR(src+i*4),4);if(!isfinite(v))return 0;}
 return 1;
}
'''
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--apply',action='store_true');args=parser.parse_args()
result=prefix+'\n'.join(out)+'\n';destination=root/'runtime/matrix_inverse_fast.c'
if args.apply:destination.write_text(result)
else:assert destination.read_text()==result,'Native helper differs from checked generator; review before --apply'
print('scalar doubles',counter,'maximum depth',maximum,'final stack delta',top)
