from pathlib import Path
import re,json,hashlib,struct,pefile,capstone
from capstone.x86_const import *
root=Path.cwd();cfg=json.loads((root/'config/runtime_seeds.json').read_text());data=(root/cfg['binary']).read_bytes();assert hashlib.sha256(data).hexdigest()==cfg['sha256'];pe=pefile.PE(data=data);base=pe.OPTIONAL_HEADER.ImageBase;m=pe.get_memory_mapped_image();md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);md.detail=True
known={int(f['va'],16) for f in json.loads((root/'build/translated/desertstorm/catalog.json').read_text())};known|={int(f['address'],16) for f in json.loads((root/'docs/evidence/recovered-targets.json').read_text())['recovered']}
def sec(a):return next((s for s in pe.sections if base+s.VirtualAddress<=a<base+s.VirtualAddress+max(s.Misc_VirtualSize,s.SizeOfRawData)),None)
def code(a):
 s=sec(a);return s is not None and (s.Characteristics&0x20000000 or s.Name.startswith(b'stxt') or s.Name.startswith(b'.text'))
def ro(a):
 s=sec(a);return s is not None and s.Name.startswith(b'.rdata') and not code(a)
def u32(a):return struct.unpack_from('<I',m,a-base)[0]
refs={}
for p in (root/'build/translated/desertstorm').glob('functions_*.c'):
 for l in p.read_text().splitlines():
  z=re.search(r'MEM32\(([^)]+)\) = 0x([0-9A-F]+)u; /\* 0x([0-9A-F]+): mov',l)
  if not z:continue
  addr=int(z[2],16);ip=int(z[3],16)
  if not ro(addr):continue
  ins=next(md.disasm(m[ip-base:ip-base+15],ip,count=1),None)
  if not ins or ins.id!=X86_INS_MOV or len(ins.operands)!=2:continue
  dst,src=ins.operands
  if dst.type!=X86_OP_MEM or dst.mem.base in (0,X86_REG_ESP,X86_REG_EBP) or src.type!=X86_OP_IMM or src.imm!=addr:continue
  if not code(u32(addr)):continue
  refs.setdefault(addr,[]).append({'instruction':hex(ip),'store':ins.op_str,'file':p.name})
tables=[]
for a,sites in sorted(refs.items()):
 targets=[]
 for k in range(128):
  q=a+k*4
  if k and q in refs:break
  v=u32(q)
  if not code(v):break
  targets.append(v)
 if len(targets)<2:continue
 missing=[{'slot':k,'target':hex(v),'bytes':m[v-base:v-base+16].hex()} for k,v in enumerate(targets) if v not in known]
 tables.append({'base':hex(a),'slots':len(targets),'references':list({s['instruction']:s for s in sites}.values()),'missing':missing})
result={'sha256':cfg['sha256'],'scope':'Explicit original MOV [nonstack register+offset],immediate in the original .rdata section (section name, not PE read-only permissions) in lifted code, verified decoded instruction. Bounded contiguous executable-pointer run ends at next verified table start or first noncode entry (max128). Candidates require per-class validation before recovery.','tables':tables,'counts':{'tables':len(tables),'slots':sum(t['slots'] for t in tables),'missing_slots':sum(len(t['missing']) for t in tables),'missing_unique':len({s['target'] for t in tables for s in t['missing']})}}
Path('docs/evidence/constructor-vtable-audit.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result['counts']));print(json.dumps([{k:t[k] for k in ['base','slots','missing','references']} for t in tables if t['missing']],indent=2))
