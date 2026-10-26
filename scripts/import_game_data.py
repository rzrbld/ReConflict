#!/usr/bin/env python3
"""Validate and copy the player's own installed game; never copy saves/configs."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import tempfile

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('source',type=Path,help='Installed Windows game directory containing DesertStorm.exe')
    p.add_argument('destination',type=Path,nargs='?',help='Port data directory; must be absent or empty')
    p.add_argument('--check-only',action='store_true')
    p.add_argument('--manifest',type=Path,default=Path(__file__).resolve().parents[1]/'config/game-files.sha256')
    args=p.parse_args();source=args.source.resolve(strict=True)
    if not source.is_dir():p.error('Source must be the installed game directory, not the installer')
    entries=[]
    for line in args.manifest.read_text().splitlines():
        expected,name=line.split('  ',1);relative=Path(name)
        if len(expected)!=64 or not all(c in '0123456789abcdef' for c in expected) or relative.is_absolute() or '..' in relative.parts:
            p.error('Invalid game-data manifest')
        entries.append((expected,relative))
    if not entries or len({str(n).casefold() for _,n in entries})!=len(entries):p.error('Empty or ambiguous manifest')
    # Accommodate case differences on Windows copies; reject ambiguous matches.
    names={}
    for path in source.rglob('*'):
        if path.is_file():names.setdefault(path.relative_to(source).as_posix().casefold(),[]).append(path)
    selected=[];errors=[]
    for expected,relative in entries:
        matches=names.get(relative.as_posix().casefold(),[])
        if len(matches)!=1:errors.append(f'{relative}: missing or ambiguous');continue
        path=matches[0]
        if not path.resolve().is_relative_to(source):errors.append(f'{relative}: symlink escapes source');continue
        if digest(path)!=expected:errors.append(f'{relative}: different game version or modified file');continue
        selected.append((path,relative,expected))
    if errors:raise SystemExit('Game data validation failed; nothing copied:\n'+'\n'.join(errors[:20])+ ('\n…' if len(errors)>20 else ''))
    print(f'Validated {len(selected)} files for the supported game version.')
    if args.check_only:return
    if args.destination is None:p.error('Specify the port data directory or use --check-only')
    destination=args.destination.absolute()
    if destination.is_symlink():p.error('Destination must not be a symlink')
    if destination.exists() and (not destination.is_dir() or any(destination.iterdir())):p.error('Destination must be absent or empty; existing files were not changed')
    destination.parent.mkdir(parents=True,exist_ok=True)
    staging=Path(tempfile.mkdtemp(prefix='.desert-storm-import-',dir=destination.parent))
    try:
        for path,relative,expected in selected:
            target=staging/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
            if digest(target)!=expected:raise RuntimeError(f'Source changed during copy: {relative}')
        if destination.exists():destination.rmdir() # empty only; fails safely if changed
        os.replace(staging,destination)
    finally:
        if staging.exists():shutil.rmtree(staging)
    print(f'Installed game data: {destination}\nSaves and port configuration were not copied or changed.')

if __name__=='__main__':main()
