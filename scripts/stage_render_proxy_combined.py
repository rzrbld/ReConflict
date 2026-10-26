#!/usr/bin/env python3
"""Combine reviewed queue-cursor patch with the reviewed client guard reset."""
from pathlib import Path
import hashlib,json,shutil,subprocess,tempfile
root=Path(__file__).resolve().parent.parent
baseline=root/'build/render-thread-stage/client-reset'
cursor=root/'build/render-thread-stage/cacheline/cursor.patch'
source=(baseline/'render_thread_proxy.c').read_bytes()
expected='c44256061bf8f1c4ed048ae961903267529b990b1aba26a3275cf53fcb6bc5ba'
sha=lambda data:hashlib.sha256(data).hexdigest()
if sha(source)!=expected:raise SystemExit('Validated guard-reset baseline changed')
patch=cursor.read_bytes()
with tempfile.TemporaryDirectory(prefix='ds-rt-combine-',dir='/tmp') as temporary:
 work=Path(temporary);(work/'probe').mkdir();(work/'probe/render_thread_proxy.c').write_bytes(source)
 result=subprocess.run(['patch','-p1','--batch','--forward'],input=patch,cwd=work,capture_output=True,check=True)
 print(result.stdout.decode().strip())
 combined=(work/'probe/render_thread_proxy.c').read_bytes()
out=root/'build/render-thread-stage/client-reset-cacheline';out.mkdir(parents=True,exist_ok=True)
(out/'render_thread_proxy.c').write_bytes(combined)
for name in ['render_proxy_ops.inc','render_proxy_wrappers.inc','render_thread_proxy.h']:shutil.copyfile(baseline/name,out/name)
shutil.copyfile(cursor,out/'cursor.patch')
(out/'manifest.json').write_text(json.dumps({'guard_reset_baseline_sha256':expected,'cursor_patch_sha256':sha(patch),'candidate_sha256':sha(combined),'files':{p.name:sha(p.read_bytes()) for p in sorted(out.iterdir()) if p.suffix in ('.c','.h','.inc')},'scope':'Guard-resetc442 plus separately validated64-byte cursor alignment/cached-consumed changes only; all baselines preserved'},indent=2)+'\n')
print(out);print('combined_sha256='+sha(combined))
