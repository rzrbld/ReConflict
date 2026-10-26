#!/usr/bin/env python3
"""Fail-closed staging of existing lifted C for RECOMP_LOCAL_REGS.

Never edits the input. Native fast-hook early returns precede local capture and
remain global-only. All post-entry returns and fallthrough flush local registers.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re

TOKEN = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_]\w*|[^\s]', re.S)
REGS = {'eax','ecx','edx','ebx','esp','esi','edi','ebp'}
HOOKS = {'ds_math_fast_540470','ds_math_fast_5405a0','ds_matrix_multiply_fast_4ac1fb'}

def tokens(source):
    return [(m.group(),m.start(),m.end()) for m in TOKEN.finditer(source)
            if not m.group().startswith(('/*','//','"',"'"))]

def functions(source):
    t=tokens(source);result=[];i=0
    while i+5<len(t):
        if [x[0] for x in t[i:i+6]]==['void',t[i+1][0],'(','void',')','{'] and t[i+1][0].startswith('recomp_'):
            depth=1;j=i+6
            while j<len(t) and depth:
                depth+=(t[j][0]=='{')-(t[j][0]=='}');j+=1
            if depth:raise ValueError('Unbalanced function '+t[i+1][0])
            result.append((t[i+1][0],t[i][1],t[j-1][2],t[i+5][2],t[j-1][1]));i=j
        else:i+=1
    return result

def transform(source,extra_hooks=()):
    if 'RECOMP_REGS_LOCALS' in source:raise ValueError('Input already contains local-register declarations')
    edits=[];report=[];allowed=HOOKS|set(extra_hooks)
    for name,start,end,opening,closing in functions(source):
        body=source[opening:closing];t=tokens(body);enters=[x for x in t if x[0]=='RECOMP_ENTER']
        if not enters:
            if any(x[0] in REGS for x in t):raise ValueError('No entry for register-using body '+name)
            report.append({'name':name,'native_only':True});continue
        if len(enters)!=1:raise ValueError('Ambiguous entry in '+name)
        entry=enters[0][1];pre=tokens(body[:entry]);early=[]
        for i,x in enumerate(pre):
            if x[0]!='return':continue
            # Only the checked native hooks may return before local capture.
            pattern=r'if\s*\(\s*('+'|'.join(map(re.escape,sorted(allowed)))+r')\s*\(\s*\)\s*\)\s*return\s*;'
            matches=[m for m in re.finditer(pattern,body[:entry]) if m.start()<=x[1]<m.end()]
            if len(matches)!=1:raise ValueError('Unreviewed pre-entry return in '+name)
            early.append(matches[0].group(1))
        # No hidden reads/writes of aliased guest registers before capture.
        if any(x[0] in REGS for x in pre):raise ValueError('Register access before entry in '+name)
        absolute=opening+entry;line_start=source.rfind('\n',0,absolute)+1
        if source[line_start:absolute].strip():raise ValueError('Entry is not on its own line in '+name)
        edits.append((line_start,line_start,'    RECOMP_REGS_LOCALS\n'))
        returns=0
        for i,x in enumerate(t):
            if x[0]=='return' and x[1]>=entry:
                if i+1>=len(t) or t[i+1][0]!=';':raise ValueError('Value return in void body '+name)
                edits.append((opening+x[1],opening+t[i+1][2],'{ RECOMP_REGS_OUT(); return; }'));returns+=1
        edits.append((closing,closing,'    RECOMP_REGS_OUT(); /* checked fallthrough flush */\n'))
        report.append({'name':name,'returns':returns,'early_native_hooks':early,
                       'setjmp':sum(x[0]=='DS_GUEST_SETJMP' for x in t),
                       'cpuid':sum(x[0]=='CPUID' for x in t)})
    if not report:raise ValueError('No lifted functions found')
    for start,end,text in sorted(edits,reverse=True):source=source[:start]+text+source[end:]
    return source,report

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('inputs',nargs='+',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    a.output.mkdir(parents=True,exist_ok=True);rows=[]
    common=Path(os.path.commonpath([str(x.resolve().parent) for x in a.inputs]))
    for src in a.inputs:
        target=a.output/src.resolve().relative_to(common)
        target.parent.mkdir(parents=True,exist_ok=True)
        if target.resolve()==src.resolve():raise SystemExit('Refusing input overwrite')
        before=src.read_text();after,report=transform(before);target.write_text(after)
        header=src.parent/"functions.h"
        if header.exists():(target.parent/header.name).write_bytes(header.read_bytes())
        rows.append({'source':str(src),'staged':str(target),'sha256':hashlib.sha256(before.encode()).hexdigest(),'functions':report})
    (a.output/'local-regs-report.json').write_text(json.dumps(rows,indent=2)+'\n');print('Staged',sum(len(x['functions']) for x in rows),'functions from',len(rows),'units')
if __name__=='__main__':main()
