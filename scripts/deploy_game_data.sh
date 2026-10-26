#!/usr/bin/env bash
# Copy the user's installed assets into the port's data directory only.
set -euo pipefail
cd "$(dirname "$0")/.."
source_dir=${GAME_SOURCE_DIR:-setup_conflict_-_desert_storm_1.0_(21657)}
target=${TARGET_SSH:?Set TARGET_SSH to your handheld SSH address}
ssh_args=(-F /dev/null -o ConnectTimeout=10)
[[ -z ${SSH_CONTROL_SOCKET:-} ]] || ssh_args+=(-S "$SSH_CONTROL_SOCKET")
mkdir -p build/game
python3 - "$source_dir" <<'PY'
import hashlib,sys
from pathlib import Path
root=Path(sys.argv[1]).resolve(strict=True)
files=sorted(p for p in root.iterdir() if p.is_file() and
             (p.name=='DesertStorm.exe' or p.suffix.lower() in {'.dll','.dat','.sch','.key'}))
files+=sorted(p for p in (root/'Data').rglob('*') if p.is_file())
if not files or not (root/'DesertStorm.exe').is_file():raise SystemExit('Game input missing')
manifest=bytearray();checks=[]
for p in files:
    if not p.resolve().is_relative_to(root):raise SystemExit(f'Asset escapes source directory: {p}')
    name=p.relative_to(root).as_posix()
    manifest+=name.encode()+b'\0'
    checks.append(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+name+'\n')
Path('build/game/assets.files0').write_bytes(manifest)
Path('build/game/assets.SHA256SUMS').write_text(''.join(checks))
print(f'Copying {len(files)} original asset files; save/config folders are separate.')
PY
ssh "${ssh_args[@]}" "$target" 'mkdir -p /mnt/sdcard/ports/desert-storm/data'
tar -C "$source_dir" --null -T build/game/assets.files0 -cf - | gzip -1 |
  ssh "${ssh_args[@]}" "$target" 'gzip -dc | tar -C /mnt/sdcard/ports/desert-storm/data -xf -'
ssh "${ssh_args[@]}" "$target" 'cat > /mnt/sdcard/ports/desert-storm/data/assets.SHA256SUMS' < build/game/assets.SHA256SUMS
