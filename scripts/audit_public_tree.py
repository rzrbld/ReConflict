#!/usr/bin/env python3
"""Audit Git-visible files, optionally export a source ZIP with no local inputs."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import zipfile

ROOT=Path(__file__).resolve().parents[1]
GENERATED={'runtime/geometry_fast.c','runtime/quaternion_fast.c','runtime/matrix_inverse_fast.c','runtime/physics_rank_update.inc'}
BLOCKED={'.exe','.dll','.dat','.sch','.key','.bin','.bmp','.png','.jpg','.jpeg','.dds','.psf','.wav','.mp3','.avi','.o','.a','.so','.aarch64','.zip','.tar','.gz','.7z','.iso','.log','.csv','.pyc'}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--export',type=Path);a=p.parse_args()
    with tempfile.TemporaryDirectory(prefix='ds-public-index-') as temp:
        probe=subprocess.run(['git','-C',str(ROOT),'rev-parse','--is-inside-work-tree'],capture_output=True)
        if probe.returncode==0:cmd=['git','-C',str(ROOT)]
        else:
            subprocess.run(['git','init','--quiet',temp],check=True)
            cmd=['git','--git-dir',str(Path(temp)/'.git'),'--work-tree',str(ROOT)]
        raw=subprocess.check_output(cmd+['ls-files','--cached','--others','--exclude-standard','-z'],cwd=ROOT)
        names=sorted(set(raw.decode().split('\0'))-{''});errors=[]
        for name in names:
            path=ROOT/name;parts=Path(name).parts
            if parts[0] in {'build','dist','docs','.agents','.codex','.aws'} or any(x in {'captures','saves','userdata','evidence','__pycache__'} for x in parts):errors.append(name)
            elif name in GENERATED or path.suffix.lower() in BLOCKED or path.name in {'AGENTS.md','SKILL.md'}:errors.append(name)
            elif path.is_symlink() or not path.is_file():errors.append(name)
            elif path.stat().st_size>8*1024*1024:errors.append(name)
            else:
                with path.open('rb') as f:magic=f.read(4)
                if magic.startswith((b'MZ',b'\x7fELF')):errors.append(name)
        if errors:raise SystemExit('Do not publish: excluded/tracked or unexpected files:\n'+'\n'.join(errors))
        print(f'PASS: {len(names)} Git-visible source/config/license files; no game assets, binaries, generated game files or internal docs.')
        if a.export:
            a.export.parent.mkdir(parents=True,exist_ok=True)
            with zipfile.ZipFile(a.export,'x',compression=zipfile.ZIP_DEFLATED) as z:
                for name in names:
                    info=zipfile.ZipInfo(name,(1980,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED
                    info.external_attr=(0o100755 if (ROOT/name).stat().st_mode&0o111 else 0o100644)<<16
                    z.writestr(info,(ROOT/name).read_bytes())
            print(f'Source export: {a.export}')

if __name__=='__main__':main()
