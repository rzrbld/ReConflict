#!/usr/bin/env python3
"""Immutable-by-default source snapshot and dependency-aware LTO build records."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import time

ROOT=Path(__file__).resolve().parent.parent
STAGE=ROOT/'build/lto-stage'
SRC=STAGE/'src'
OUT=STAGE/'output'
COMMON='-std=gnu11 -flto=auto -fno-strict-aliasing -fwrapv -mcpu=cortex-a53 -DRECOMP_FP_STACK_EXACT=1 -Werror=implicit-function-declaration'
def digest(data):return hashlib.sha256(data).hexdigest()
def origin(relative):
    if relative.startswith('translated/'):return ROOT/'build'/relative
    if relative.startswith(('runtime/','third_party/')):return ROOT/relative
    raise SystemExit('Unsupported snapshot path: '+relative)
def inputs():
    result={}
    for p in sorted((ROOT/'build/translated').rglob('*')):
        if p.is_file() and p.suffix in ('.c','.h'):result[str(p.relative_to(ROOT/'build'))]=p
    for p in sorted((ROOT/'runtime').iterdir()):
        if p.is_file() and p.suffix in ('.h','.inc'):result[str(p.relative_to(ROOT))]=p
    for p in sorted((ROOT/'third_party/pcrecomp/runtime/recomp32').rglob('*.h')):result[str(p.relative_to(ROOT))]=p
    for name in ('cpu_features.c','unsupported.c'):result['runtime/'+name]=ROOT/'runtime'/name
    return result
def snapshot(refresh):
    SRC.mkdir(parents=True,exist_ok=True);OUT.mkdir(parents=True,exist_ok=True)
    path=STAGE/'inputs.json'
    if not path.exists():
        if refresh:raise SystemExit('Create initial snapshot before refreshing')
        entries={}
        for relative,p in inputs().items():
            data=p.read_bytes();target=SRC/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
            entries[relative]={'sha256':digest(data),'bytes':len(data)}
        manifest={'created_unix':time.time(),'inputs':entries,'refreshes':[]}
    else:
        manifest=json.loads(path.read_text())
        for relative,entry in manifest['inputs'].items():
            if digest((SRC/relative).read_bytes())!=entry['sha256']:raise SystemExit('Snapshot modified outside refresh: '+relative)
        changed={}
        for relative in refresh:
            if relative.startswith('build/'):relative=relative[6:]
            if '..' in Path(relative).parts or Path(relative).is_absolute():raise SystemExit('Invalid refresh path')
            p=origin(relative)
            if p.suffix not in ('.c','.h','.inc') or not p.is_file():raise SystemExit('Refresh requires existing C/H/INC file: '+relative)
            data=p.read_bytes();target=SRC/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
            entry={'sha256':digest(data),'bytes':len(data)}
            if entry!=manifest['inputs'].get(relative):changed[relative]={'before':manifest['inputs'].get(relative),'after':entry}
            manifest['inputs'][relative]=entry
        if changed:manifest['refreshes'].append({'unix':time.time(),'changed':changed})
    path.write_text(json.dumps(manifest,indent=2)+'\n')
    print(f"Snapshot: {len(manifest['inputs'])} files, {sum(v['bytes'] for v in manifest['inputs'].values())} bytes")
def flags(relative):return COMMON+(' -ffp-contract=off' if relative=='translated/desertstorm/functions_0097.c' else '')+' -O'+('1' if relative=='translated/mission8/functions_0000.c' else '2')
def object_path(relative):
    p=Path(relative)
    return OUT/(str(p.with_suffix('.aarch64.o')).removeprefix('translated/') if relative.startswith('translated/') else 'support/'+p.with_suffix('.aarch64.o').name)
def dependencies(relative):
    p=object_path(relative).with_suffix('.d')
    if not p.exists():return None
    text=p.read_text().replace('\\\n',' ')
    result=[]
    for item in text.split(':',1)[1].split():
        if not item.startswith('/input/'):raise SystemExit('Unexpected dependency: '+item)
        name=item[len('/input/'):];target=SRC/name
        if not target.is_file():raise SystemExit('Missing staged dependency '+name)
        result.append((name,digest(target.read_bytes())))
    return sorted(set(result))
def fingerprint(relative):
    deps=dependencies(relative)
    if deps is None:return None
    compiler=(OUT/'compiler.txt').read_text()
    return {'flags':flags(relative),'compiler':compiler,'inputs':deps}
def jobs(update,run_id):
    manifest=json.loads((STAGE/'inputs.json').read_text())
    sources=sorted(p for p in manifest['inputs'] if p.endswith('.c'))
    dirty=[]
    for relative in sources:
        obj=object_path(relative);meta=obj.with_suffix('.inputs.json');current=fingerprint(relative)
        if update:
            if run_id and (not obj.with_suffix('.done').exists() or obj.with_suffix('.done').read_text().strip()!=run_id):continue
            if obj.is_file() and current:meta.write_text(json.dumps(current,indent=2)+'\n')
        elif not obj.is_file() or not meta.is_file() or json.loads(meta.read_text())!=json.loads(json.dumps(current)):
            dirty.append(relative)
    if not update:
        (OUT/'compile-jobs.txt').write_text(''.join(p+'\n' for p in dirty))
        print(f"LTO jobs: {len(dirty)}/{len(sources)} (mission8 dispatcher retains O1)")
    else:
        (OUT/'build-summary.json').write_text(json.dumps({'finished_unix':time.time(),'source_count':len(sources),'common_flags':COMMON,'archives':sorted(str(p.relative_to(OUT)) for p in OUT.glob('*/lifted.aarch64.a'))},indent=2)+'\n')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--refresh',action='append',default=[]);p.add_argument('--jobs',action='store_true');p.add_argument('--record-success',action='store_true');p.add_argument('--run-id');a=p.parse_args()
    if a.jobs or a.record_success:jobs(a.record_success,a.run_id)
    else:snapshot(a.refresh)
