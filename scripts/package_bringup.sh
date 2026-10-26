#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/package/desert-storm-bringup/LICENSES dist
stage=$PWD/build/package
for name in platform-probe.aarch64 desert-setup.aarch64; do
    readelf -h "build/probe/$name" | grep -q 'Machine:.*AArch64'
    install -m 755 "build/probe/$name" "$stage/desert-storm-bringup/$name"
done
install -m 755 'packaging/Desert Storm Setup.sh' "$stage/Desert Storm Setup.sh"
install -m 644 packaging/controller-buttons.cfg "$stage/desert-storm-bringup/"
install -m 644 packaging/README.txt "$stage/desert-storm-bringup/"
install -m 644 third_party/ConflictDesertStormPatch/LICENSE "$stage/desert-storm-bringup/LICENSES/ConflictDesertStormPatch.txt"
# Explicit allowlist: excludes firmware libraries, lifted game code and assets.
python3 - <<'PY'
from pathlib import Path
import zipfile
stage = Path('build/package')
files = [stage / p for p in ['Desert Storm Setup.sh',
    'desert-storm-bringup/platform-probe.aarch64', 'desert-storm-bringup/desert-setup.aarch64',
    'desert-storm-bringup/controller-buttons.cfg', 'desert-storm-bringup/README.txt',
    'desert-storm-bringup/LICENSES/ConflictDesertStormPatch.txt']]
with zipfile.ZipFile('dist/desert-storm-bringup.zip', 'w', zipfile.ZIP_DEFLATED) as z:
    for f in files:
        if f.is_file():
            info = zipfile.ZipInfo(f.relative_to(stage).as_posix(), (2026, 10, 26, 0, 0, 0))
            info.external_attr = f.stat().st_mode << 16
            z.writestr(info, f.read_bytes(), compress_type=zipfile.ZIP_DEFLATED)
PY
sha256sum dist/desert-storm-bringup.zip > dist/SHA256SUMS
