#!/usr/bin/env bash
# Isolated native geometry flag correction; no active objects/game/deployment.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import hashlib,json,shutil
root=Path.cwd();out=root/'build/render-thread-stage/geometry-flags';out.mkdir(parents=True,exist_ok=True)
files={'geometry_fast.c':root/'runtime/geometry_fast.c','test.c':root/'tests/geometry_fast.c','reference.c':root/'build/geometry-math-stage/reference.c'}
frozen=root/'build/render-thread-game/snapshot'
for p in (frozen/'third_party/pcrecomp/runtime/recomp32').glob('*.h'):files['include/'+p.name]=p
for p in (frozen/'runtime').glob('*.h'):files['include/'+p.name]=p
manifest={}
for name,p in files.items():
 data=p.read_bytes();target=out/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
 manifest[name]={'origin':str(p.relative_to(root)),'sha256':hashlib.sha256(data).hexdigest()}
(out/'inputs.json').write_text(json.dumps(manifest,indent=2)+'\n')
PY
out=build/render-thread-stage/geometry-flags
flags='-std=gnu11 -O2 -fno-strict-aliasing -fwrapv -ffp-contract=off'
printf '%s\n' "$flags -g -mcpu=cortex-a53" > "$out/object-flags.txt"
"${CC:-cc}" $flags -I"$out/include" "$out/test.c" "$out/geometry_fast.c" "$out/reference.c" -lm -o "$out/test-host"
"$out/test-host" --disabled > "$out/host-disabled.log"
"$out/test-host" > "$out/host-differential.log"
image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -v "$PWD/$out:/out:rw" "$image" bash -eu -o pipefail -c '
 flags="-std=gnu11 -O2 -fno-strict-aliasing -fwrapv -ffp-contract=off -mcpu=cortex-a53"
 aarch64-linux-gnu-gcc $flags -g -I/out/include -c /out/geometry_fast.c -o /out/ds_geometry_fast.o
 aarch64-linux-gnu-gcc $flags -I/out/include /out/test.c /out/geometry_fast.c /out/reference.c -lm -o /out/geometry-flags-test.aarch64
 '
readelf -h "$out/ds_geometry_fast.o" > "$out/object-elf-header.txt"
readelf -h "$out/geometry-flags-test.aarch64" > "$out/test-elf-header.txt"
sha256sum "$out/ds_geometry_fast.o" "$out/geometry-flags-test.aarch64" > "$out/SHA256SUMS"
