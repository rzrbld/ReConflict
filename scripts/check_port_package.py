#!/usr/bin/env python3
"""Local structural checks, not a substitute for PortMaster community review."""
import json
from pathlib import Path, PurePosixPath
import re
import struct
import sys
import xml.etree.ElementTree as ET
import zipfile

def validate(files):
    def need(condition, message):
        if not condition:
            raise ValueError(message)
    for name in files:
        p = PurePosixPath(name)
        need(not p.is_absolute() and '..' not in p.parts, 'Unsafe archive path: '+name)
    required = {'port.json', 'README.md', 'gameinfo.xml', 'screenshot.png',
                'ReConflict.sh', 'ReConflict Settings.sh'}
    need(required <= files.keys(), 'Missing PortMaster entry files')
    need(all(n in required or n.startswith('reconflict/') for n in files),
         'Unowned top-level file or folder')
    meta = json.loads(files['port.json'])
    need(meta['version'] == 4 and meta['name'] == 'reconflict.zip', 'Bad metadata version/name')
    need(re.fullmatch(r'[a-z0-9][a-z0-9._]*', meta['name'][:-4]), 'Invalid port name')
    need(set(meta['items']) == {'ReConflict.sh', 'ReConflict Settings.sh', 'reconflict'},
         'Install items do not match package')
    attr = meta['attr']
    need(attr['arch'] == ['aarch64'] and attr['min_glibc'] == '2.36', 'Incorrect target requirements')
    need(attr['rtr'] is False and attr['exp'] is True and attr['availability'] == 'commercial',
         'Commercial/experimental status missing')
    picture = files['screenshot.png']
    need(len(picture) >= 24 and picture[:8] == b'\x89PNG\r\n\x1a\n', 'Not a PNG')
    w,h = struct.unpack('>II', picture[16:24])
    need(w >= 640 and h >= 480 and w*3 == h*4, 'Screenshot must be >=640x480, 4:3')
    need(files.get('reconflict/screenshot.png') == picture, 'Installed screenshot missing')
    game = ET.fromstring(files['gameinfo.xml']).find('game')
    need(game is not None and game.findtext('path') == './ReConflict.sh', 'Invalid XML script path')
    need(game.findtext('image') == './reconflict/screenshot.png', 'Invalid XML image path')
    for name in ['port-MIT', 'innoextract', 'Vurus', 'pcrecomp', 'ConflictDesertStormPatch',
                 'GCC-runtime', 'GPL-3', 'SDL2', 'SDL2_ttf', 'libboost-filesystem1.74.0',
                 'libboost-iostreams1.74.0', 'libboost-program-options1.74.0']:
        need(len(files.get('reconflict/licenses/'+name+'.txt', b'')) > 100, 'Missing license: '+name)
    for name, content in files.items():
        need(not name.endswith(('.exe', '.dll', '.py')) and '/data/' not in name,
             'Original game files or runtime Python in package: '+name)
        if '.so.' in name:
            need(name.startswith('reconflict/libs.aarch64/'), 'Nonstandard library directory')
        if content[:4] == b'\x7fELF':
            need(content[:6] == b'\x7fELF\x02\x01' and int.from_bytes(content[18:20], 'little') == 183,
                 'Non-AArch64 ELF: '+name)
    for name in ['ReConflict.sh', 'ReConflict Settings.sh']:
        need(files[name].startswith(b'#!/bin/bash\n') and b'\r' not in files[name], 'Invalid shell format')
    return len(files)

if __name__ == '__main__':
    with zipfile.ZipFile(sys.argv[1]) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise SystemExit('Duplicate ZIP entries')
        count = validate({name: archive.read(name) for name in names})
        for info in archive.infolist():
            if info.filename.endswith(('.sh', '.aarch64')) or info.filename == 'reconflict/tools/innoextract':
                if not (info.external_attr >> 16) & 0o111:
                    raise SystemExit('Missing executable permission: '+info.filename)
    print(f'PASS: {count} files; layout, metadata, screenshot geometry, notices, ARM ELF and permissions')
