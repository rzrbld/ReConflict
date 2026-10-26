"""Fixed original factorize/rank-update anchors shared by checker and patcher."""
from pathlib import Path
import hashlib,re,struct
ROOT=Path(__file__).resolve().parents[1]
SHA='d712f5ec82c14da66b098332b3cc9c99689091f46317112ea35dfd393dc3e164'
FUNCTION_SHA={'00595B10':'d37394fe0b5e870dc622fe76c139b84d3439accea4702c3c4e6f9d2ed92c2a67','00595B59':'3a65a512c048766ede91ad027efec3c362d002c14a8affe2801f7c10f8086f20'}
ANCHOR='L_00595BC5:\n'
HOOK='''    if(ds_physics_rank_update_eligible()) { /* opt-in checked factorize rank update */
#include "physics_rank_update.inc"
        goto L_00595C4C;
    }
'''
HEADER='#include "physics_fast.h"'
def validate_binary():
 data=(ROOT/'setup_conflict_-_desert_storm_1.0_(21657)/DesertStorm.exe').read_bytes()
 if hashlib.sha256(data).hexdigest()!=SHA:raise ValueError('Unsupported original PE SHA256')
 pe=struct.unpack_from('<I',data,0x3c)[0];count=struct.unpack_from('<H',data,pe+6)[0];optional=struct.unpack_from('<H',data,pe+20)[0];base=struct.unpack_from('<I',data,pe+52)[0]
 for va,prefix in [(0x595B10,bytes.fromhex('558bec83ec54535657')),(0x595BC5,bytes.fromhex('d90496d9449604'))]:
  rva=va-base;found=False
  for i in range(count):
   at=pe+24+optional+40*i;size,start,rawsize,raw=struct.unpack_from('<IIII',data,at+8)
   if start<=rva and rva+len(prefix)<=start+rawsize:
    if data[raw+rva-start:raw+rva-start+len(prefix)]!=prefix:raise ValueError('PE entry mismatch '+hex(va))
    found=True;break
  if not found:raise ValueError('Unmapped PE anchor')
def extract(source,va):
 found=re.search(r'^void recomp_desertstorm_'+va+r'\(void\) \{.*?(?=^void |\Z)',source,re.M|re.S)
 if not found:raise ValueError('Missing function '+va)
 return found.group()
def original_body(body,va):
 if HOOK in body:
  if body.count(HOOK)!=1 or ANCHOR+HOOK not in body:raise ValueError('Unexpected physics hook')
  body=body.replace(HOOK,'',1)
 if 'ds_physics_rank_update_eligible' in body or 'physics_rank_update.inc' in body:raise ValueError('Unrecognized physics hook')
 if hashlib.sha256(body.encode()).hexdigest()!=FUNCTION_SHA[va]:raise ValueError('Changed original factorize body '+va)
 return body
