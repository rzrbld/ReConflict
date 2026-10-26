#!/usr/bin/env python3
"""Bounded checked scalarization: only two call-free guest routines; no algebra rewrite."""
from pathlib import Path
import re,hashlib,json,struct,argparse
parser=argparse.ArgumentParser();parser.add_argument("--apply",action="store_true");args=parser.parse_args()
root=Path(__file__).resolve().parents[1];stage=root/'build/geometry-math-stage';stage.mkdir(parents=True,exist_ok=True)
binary=root/'setup_conflict_-_desert_storm_1.0_(21657)/DesertStorm.exe'
data=binary.read_bytes();assert hashlib.sha256(data).hexdigest()=='d712f5ec82c14da66b098332b3cc9c99689091f46317112ea35dfd393dc3e164'
expected={'004BB500':('0042','c3e9cbc65537008669dea44222bce00d0e3bef41e4a73b1dd230413d15b5e362'),'005409A0':('0072','d125c0e02ffbe7ae6f67c9ae437a32951ca150c7b6a1d83cc18ac894f88674b5')}
prefix='#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\n#ifdef __FAST_MATH__\n#error Ordered floating point required\n#endif\n'
guards=r"""
static int enabled(void){static int init,value;if(!init){const char*s=getenv("DS_FAST_MATH");value=s&&!strcmp(s,"1");init=1;}return value;}
static int overlap(uint32_t a,uint32_t n,uint32_t b,uint32_t m){return (uint64_t)a+n>b&&(uint64_t)b+m>a;}
static int eligible(int plane){
 if(!enabled())return 0;
 uint32_t s=g_esp,m=g_ecx;if(s<128||s>UINT32_MAX-16||!m||m>UINT32_MAX-68)return 0;
 if(!plane){uint32_t flags=MEM32(m+64);if(flags==1)return 1;}
 uint32_t a=MEM32(s+4),b=MEM32(s+8);if(!a||!b||a>UINT32_MAX-12||b>UINT32_MAX-12)return 0;
 uint32_t mn=plane?16:68;
 if(overlap(m,mn,s-128,144)||overlap(a,12,s-128,144)||overlap(b,12,s-128,144)||overlap(m,mn,a,12)||overlap(m,mn,b,12)||overlap(a,12,b,12))return 0;
 for(unsigned i=0;i<(plane?0:16);++i)if(!isfinite(*(float*)ADDR(m+4*i)))return 0;
 for(unsigned i=0;i<3;++i)if(!isfinite(*(float*)ADDR(a+4*i))||!isfinite(*(float*)ADDR(b+4*i)))return 0;
 return 1;
}
"""
refs=[];native=[];manifest={}
pe=struct.unpack_from('<I',data,0x3c)[0];sections=struct.unpack_from('<H',data,pe+6)[0];optional=struct.unpack_from('<H',data,pe+20)[0];base=struct.unpack_from('<I',data,pe+24+28)[0]
for va,(unit,digest) in expected.items():
 source=(root/f'build/translated/desertstorm/functions_{unit}.c').read_text();f=re.search(r'^void recomp_desertstorm_'+va+r'\(void\) \{.*?(?=^void |\Z)',source,re.M|re.S).group()
 f=re.sub(r'^    if\(ds_geometry_fast_'+va.lower()+r'\(\)\)return; /\* opt-in verified geometry fast path \*/\n','',f,flags=re.M)
 assert hashlib.sha256(f.encode()).hexdigest()==digest
 assert 'RECOMP_CALL(' not in f and 'RECOMP_ICALL(' not in f
 rva=int(va,16)-base
 for i in range(sections):
  off=pe+24+optional+i*40;size,start,rawsize,raw=struct.unpack_from('<IIII',data,off+8)
  if start<=rva<start+max(size,rawsize):entry=data[raw+rva-start:raw+rva-start+16].hex();break
 assert entry.startswith({'004BB500':'83ec1c8bc1','005409A0':'8b414083ec0c'}[va])
 refs.append(f.replace('recomp_desertstorm_'+va,'reference_'+va))
 out=[];stack=[];shadow=[];top=0;counter=0;maximum=0
 def reset():
  global stack,shadow,top
  stack=[f'g_st[{i}]' for i in range(8)];shadow=[f'g_st_i64[{i}]' for i in range(8)];top=0
 def subst(s):return re.sub(r'_st\[(\d)\]',lambda m:stack[int(m[1])],s)
 def pop():
  global top
  v=stack.pop(0);stack.append('0.0');shadow.pop(0);shadow.append('0');top-=1;return v
 def new(expr):
  global counter
  counter+=1;v=f'v{counter}';out.append(f'    double {v} = {expr};');return v
 def spill():
  assert top==0,top
  for i in range(8):
   if stack[i]!=f'g_st[{i}]':out.append(f'    g_st[{i}] = {stack[i]};')
   if shadow[i]!=f'g_st_i64[{i}]':out.append(f'    g_st_i64[{i}] = {shadow[i]};')
 reset()
 for line in f.splitlines():
  code=line.split('/*')[0].strip()
  if code.startswith('L_'):
   spill();reset();out.append(line);continue
  if code.startswith('fp_push('):
   v=new(subst(code[len('fp_push('):-2]));stack=[v]+stack[:7];shadow=['0']+shadow[:7];top+=1;maximum=max(maximum,top);continue
  if code.startswith('fp_xch('):
   idx=int(re.search(r'\((\d)\)',code)[1]);stack[0],stack[idx]=stack[idx],stack[0];shadow[0],shadow[idx]=shadow[idx],shadow[0];continue
  if 'fp_pop()' in code:
   v=stack[0]
   if code.startswith('{ double _v = fp_pop();'):pop();expression=subst(code.replace('fp_pop()',v))
   else:expression=subst(code.replace('fp_pop()',v));pop()
   out.append('    '+expression);continue
  if code.startswith('_st['):
   m=re.fullmatch(r'_st\[(\d)\] (\*=|/=|\+=|-=|=) (.*);',code);assert m,code
   idx=int(m[1]);expr=subst(m[3]);expr=expr if m[2]=='=' else stack[idx]+' '+m[2][0]+' ('+expr+')';stack[idx]=new(expr);continue
  if 'return;' in code:spill();reset()
  assert '_st[' not in code and 'fp_' not in code,code
  out.append(line.replace('recomp_desertstorm_'+va,'ds_geometry_fast_'+va.lower()))
 spill();assert maximum<=8
 body='\n'.join(out).replace('void ds_geometry_fast_'+va.lower()+'(void) {','int ds_geometry_fast_'+va.lower()+'(void) {\n    if(!eligible('+('1' if va=='004BB500' else '0')+'))return 0;').replace('return;','return 1;')
 native.append(body);manifest[va]={'source_sha256':digest,'entry16':entry,'scalar_doubles':counter,'maximum_stack_depth':maximum}
(stage/'reference.c').write_text(prefix+'\n'.join(refs));destination=root/'runtime/geometry_fast.c';result=prefix+guards+'\n'.join(native)
if args.apply:destination.write_text(result)
else:assert destination.read_text()==result,'Native helper differs from checked generation; review before --apply'
(stage/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest))
