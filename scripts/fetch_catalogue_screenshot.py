#!/usr/bin/env python3
"""Fetch only a SHA-pinned gameplay catalogue image; never reuse release code."""
import hashlib
import io
from pathlib import Path
import urllib.request
import zipfile

URL = 'https://github.com/rzrbld/ReConflict/releases/download/v20261103-154650/reconflict.zip'
ARCHIVE_SHA = '0c3d4b8f36a1b2ee0d5388ddcc966b1ed86fa26ec92936d45fbea306b77852d6'
IMAGE_SHA = '63354a323153b50023a5a65c0f75c31d65f41c0c42cd8c5190661838a41958ae'

def main():
    target = Path(__file__).resolve().parents[1]/'build/catalogue/screenshot.png'
    if target.exists() and hashlib.sha256(target.read_bytes()).hexdigest() == IMAGE_SHA:
        print(target); return
    with urllib.request.urlopen(URL, timeout=60) as response:
        archive = response.read(40*1024*1024+1)
    if hashlib.sha256(archive).hexdigest() != ARCHIVE_SHA:
        raise SystemExit('Catalogue archive checksum mismatch; pass your own gameplay screenshot instead')
    with zipfile.ZipFile(io.BytesIO(archive)) as z:
        picture = z.read('screenshot.png')
    if hashlib.sha256(picture).hexdigest() != IMAGE_SHA:
        raise SystemExit('Catalogue screenshot checksum mismatch')
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(picture)
    print(target)

if __name__ == '__main__': main()
