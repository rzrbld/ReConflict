#!/usr/bin/env python3
"""Generate a user's checksum report without changing trusted version guards."""
import argparse
import hashlib
from pathlib import Path

def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024*1024), b''):
            h.update(block)
    return h.hexdigest()

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', type=Path, help='Installer file or extracted game folder')
    p.add_argument('--output', type=Path, required=True, help='New report file; never overwrites')
    a = p.parse_args()
    manifest = Path(__file__).resolve().parents[1]/'config/game-files.sha256'
    expected = [line.split('  ', 1) for line in manifest.read_text().splitlines()]
    rows = []; errors = []
    if a.source.is_file():
        actual = digest(a.source)
        rows.append(f'{actual}  {a.source.name}')
        if actual != 'f840e10a7da55a71e89e5d83815c3f3e2bdec1206b20471291d87a3a02ea81aa':
            errors.append('Installer differs from supported English GOG build21657')
    else:
        for wanted, name in expected:
            file = a.source/name
            if not file.is_file():
                errors.append('Missing: '+name); continue
            actual = digest(file); rows.append(f'{actual}  {name}')
            if actual != wanted: errors.append('Different: '+name)
    with a.output.open('x') as out:
        out.write('\n'.join(rows)+'\n')
    print(f'Wrote {len(rows)} checksums to {a.output}')
    if errors:
        raise SystemExit('\n'.join(errors)+'\nReport only: do not replace the trusted manifest. Other versions require porting work.')
    print('Matches supported game inputs. Trusted source checks were not changed.')

if __name__ == '__main__': main()
