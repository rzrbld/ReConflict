#!/usr/bin/env python3
"""Fetch SHA-pinned Debian ARM64 packages; extract locally, never install them."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
out = ROOT / 'build/innoextract-bundle'
for directory in ['debs', 'root', 'tools/lib', 'tools/licenses']:
    (out / directory).mkdir(parents=True, exist_ok=True)
for spec in json.loads((ROOT / 'config/extractor-packages.json').read_text()):
    deb = out / 'debs' / spec['filename']
    if not deb.exists():
        temp = deb.with_suffix('.download')
        urllib.request.urlretrieve(spec['url'], temp)
        if hashlib.sha256(temp.read_bytes()).hexdigest() != spec['sha256']:
            raise SystemExit('Download checksum mismatch: ' + spec['filename'])
        temp.replace(deb)
    if hashlib.sha256(deb.read_bytes()).hexdigest() != spec['sha256']:
        raise SystemExit('Checksum mismatch: ' + str(deb))
    subprocess.run(['dpkg-deb', '-x', str(deb), str(out / 'root')], check=True)
shutil.copy2(out / 'root/usr/bin/innoextract', out / 'tools/innoextract')
for lib in (out / 'root/usr/lib/aarch64-linux-gnu').glob('libboost_*.so.*'):
    shutil.copy2(lib, out / 'tools/lib' / lib.name)
for notice in (out / 'root/usr/share/doc').glob('*/copyright'):
    shutil.copyfile(notice, out / 'tools/licenses' / (notice.parent.name + '.txt'))
print(out / 'tools')
