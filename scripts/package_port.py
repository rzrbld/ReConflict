#!/usr/bin/env python3
"""Build a local muOS runtime package from an explicitly selected ARM binary.
No Windows executable, DLL, asset, save or development evidence is packaged.
The ARM binary itself contains statically translated game logic; keep it out
of the public source tree. This script does not publish a release.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile
ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--binary',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--launcher',type=Path,default=ROOT/'build/port-launcher/desert-storm-launcher.aarch64')
    p.add_argument('--extractor-tools',type=Path,default=ROOT/'build/innoextract-bundle/tools')
    p.add_argument('--version',required=True,help='UTC release version: vYYYYMMDD-HHMMSS')
    p.add_argument('--screenshot',type=Path,default=ROOT/'build/port-launcher/previews/reconflict-menu.png')
    a=p.parse_args();binary=a.binary.read_bytes()
    if binary[:6]!=b'\x7fELF\x02\x01' or int.from_bytes(binary[18:20],'little')!=183:p.error('Expected an ELF64 little-endian AArch64 executable')
    import re
    if not re.fullmatch(r'v\d{8}-\d{6}',a.version):p.error('Expected UTC version vYYYYMMDD-HHMMSS')
    port='reconflict/'
    files={port+'desert-storm.aarch64':(binary,0o755),'README.md':((ROOT/'README.md').read_bytes(),0o644)}
    launcher=a.launcher.read_bytes()
    extractor=(a.extractor_tools/'innoextract').read_bytes()
    for name,content in [('launcher',launcher),('innoextract',extractor)]:
        if content[:6]!=b'\x7fELF\x02\x01' or int.from_bytes(content[18:20],'little')!=183:p.error(name+' must be AArch64 ELF64')
    files[port+'desert-storm-launcher.aarch64']=(launcher,0o755)
    files[port+'tools/innoextract']=(extractor,0o755)
    for name in ['libboost_filesystem.so.1.74.0','libboost_iostreams.so.1.74.0','libboost_program_options.so.1.74.0']:
        files[port+'tools/lib/'+name]=((a.extractor_tools/'lib'/name).read_bytes(),0o644)
    for name in ['innoextract','libboost-filesystem1.74.0','libboost-iostreams1.74.0','libboost-program-options1.74.0']:
        files[port+'tools/licenses/'+name+'.txt']=((a.extractor_tools/'licenses'/(name+'.txt')).read_bytes(),0o644)
    for name in ['prepare_game.py','import_game_data.py']:
        files[port+name]=((ROOT/'scripts'/name).read_bytes(),0o644)
    files[port+'README.md']=((ROOT/'README.md').read_bytes(),0o644)
    files[port+'licenses/port-MIT.txt']=((ROOT/'LICENSE').read_bytes(),0o644)
    for name in ['innoextract','Vurus']:
        files[port+'licenses/'+name+'.txt']=((ROOT/'licenses'/(name+'.txt')).read_bytes(),0o644)
    files[port+'extractor-packages.json']=((ROOT/'config/extractor-packages.json').read_bytes(),0o644)
    for name in ['ReConflict.sh','ReConflict Settings.sh']:
        files[name]=((ROOT/'packaging'/name).read_bytes(),0o755)
    for name in ['muos-session.sh','cpu-session.sh','capture-scene.sh','controller-buttons.cfg']:
        files[port+name]=((ROOT/'packaging'/name).read_bytes(),0o755 if name.endswith('.sh') else 0o644)
    files[port+'graphics-low.cfg']=((ROOT/'packaging/graphics-low.cfg').read_bytes(),0o644)
    files[port+'game-files.sha256']=((ROOT/'config/game-files.sha256').read_bytes(),0o644)
    for upstream in ['pcrecomp','ConflictDesertStormPatch']:
        license_path=ROOT/'licenses/ConflictDesertStormPatch.txt' if upstream=='ConflictDesertStormPatch' else ROOT/'third_party/pcrecomp/LICENSE'
        files[port+'licenses/'+upstream+'.txt']=(license_path.read_bytes(),0o644)
    files[port+'BUILD.json']=(json.dumps({'version':a.version,'architecture':'aarch64','resolution':'640x480','executable_sha256':hashlib.sha256(binary).hexdigest(),'launcher_sha256':hashlib.sha256(launcher).hexdigest(),'innoextract_sha256':hashlib.sha256(extractor).hexdigest(),'requires_player_game_files':True},indent=2).encode()+b'\n',0o644)
    metadata=json.loads((ROOT/'packaging/port.json').read_text())
    files['port.json']=(json.dumps(metadata,indent=2).encode()+b'\n',0o644)
    files['gameinfo.xml']=((ROOT/'packaging/gameinfo.xml').read_bytes(),0o644)
    picture=a.screenshot.read_bytes()
    if not picture.startswith(b'\x89PNG\r\n\x1a\n'):p.error('Expected the asset-free launcher PNG preview')
    files['screenshot.png']=(picture,0o644)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(a.output,'x',compression=zipfile.ZIP_DEFLATED) as z:
        for name,(content,mode) in sorted(files.items()):
            info=zipfile.ZipInfo(name,(1980,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=(0o100000|mode)<<16;z.writestr(info,content)
    print(f'{a.output}: {len(files)} files; original game files and saves excluded')

if __name__=='__main__':main()
