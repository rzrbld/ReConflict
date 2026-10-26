#!/usr/bin/env bash
# Explicit snapshot/relink experiment. Never overwrites normal objects/game.
# First invocation snapshots current frozen native objects and target libraries.
# Later invocations reuse that snapshot; remove/rename it explicitly to replace.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import hashlib,json,shutil,time
root=Path.cwd();stage=root/'build/lto-stage';native=stage/'native-objects';libs=stage/'target-libs'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
manifest=stage/'native-inputs.json'
if manifest.exists():
    old=json.loads(manifest.read_text())
    for item in old['files']:
        p=stage/item['staged']
        if not p.is_file() or sha(p)!=item['sha256']:raise SystemExit('Frozen native input changed: '+str(p))
    print('Reusing frozen native objects/libraries from '+old['normal_game_sha256'])
else:
    if native.exists() or libs.exists():raise SystemExit('Incomplete native snapshot exists; inspect before removing it')
    objects=sorted((root/'build/game/obj').glob('*.o'))
    libraries=sorted(p for p in (root/'build/target-libs').iterdir() if p.is_file())
    if not objects:raise SystemExit('No frozen native runtime objects')
    game=root/'build/game/desert-storm.aarch64';game_hash=sha(game)
    expected=(root/'build/game/SHA256SUMS').read_text().split()[0]
    if game_hash!=expected:raise SystemExit('Normal executable hash does not match manifest')
    original={p:sha(p) for p in objects+libraries}
    native.mkdir();libs.mkdir();files=[]
    for p,h in original.items():
        target=(native if p in objects else libs)/p.name
        shutil.copy2(p,target,follow_symlinks=True)
        if sha(target)!=h:raise SystemExit('Native snapshot copy mismatch: '+str(p))
        files.append({'origin':str(p.relative_to(root)),'staged':str(target.relative_to(stage)),'sha256':h,'bytes':target.stat().st_size})
    if any(sha(p)!=h for p,h in original.items()) or sha(game)!=game_hash:raise SystemExit('Native inputs changed during snapshot; do not link')
    manifest.write_text(json.dumps({'created_unix':time.time(),'normal_game_sha256':game_hash,'files':files},indent=2)+'\n')
    print(f'Frozen {len(objects)} native objects and {len(libraries)} library files; normal executable {game_hash}')
PY
python3 scripts/stage_lto.py --jobs
[[ ! -s build/lto-stage/output/compile-jobs.txt ]] || { echo 'Staged LTO archives need rebuilding' >&2; exit 1; }
sha256sum -c build/lto-stage/output/ARCHIVE-SHA256SUMS
mkdir -p build/lto-stage/game
image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
started=$(date +%s)
status=0
docker run --rm --network none --read-only --tmpfs /tmp --memory=64g --memory-swap=64g \
  --user "$(id -u):$(id -g)" -v "$PWD/build/lto-stage/output:/archives:ro" \
  -v "$PWD/build/lto-stage/native-objects:/native:ro" -v "$PWD/build/lto-stage/target-libs:/libs:ro" \
  -v "$PWD/build/lto-stage/game:/game:rw" "$image" bash -eu -o pipefail -c '
    aarch64-linux-gnu-gcc --version > /game/compiler.txt
    cmp /game/compiler.txt /archives/compiler.txt
    [[ "$(aarch64-linux-gnu-gcc -dumpversion)" == 12 ]]
    archives=()
    for module in /archives/*; do [[ -d "$module" && -f "$module/lifted.aarch64.a" ]] && archives+=("$module/lifted.aarch64.a"); done
    printf "%s\n" "aarch64-linux-gnu-gcc -g -rdynamic -O2 -flto=8 -fno-strict-aliasing -fwrapv -mcpu=cortex-a53 -o /game/desert-storm-lto.aarch64.new /native/*.o -Wl,--start-group ${archives[*]} -Wl,--end-group -L/libs -Wl,--allow-shlib-undefined -lSDL2 -lSDL2_ttf -lGLESv2 -lm -lpthread -ldl" > /game/link-command.txt
    aarch64-linux-gnu-gcc -g -rdynamic -O2 -flto=8 -fno-strict-aliasing -fwrapv -mcpu=cortex-a53 \
      -o /game/desert-storm-lto.aarch64.new /native/*.o \
      -Wl,--start-group "${archives[@]}" -Wl,--end-group \
      -L/libs -Wl,--allow-shlib-undefined -lSDL2 -lSDL2_ttf -lGLESv2 -lm -lpthread -ldl \
      > /game/link.log 2>&1
    mv /game/desert-storm-lto.aarch64.new /game/desert-storm-lto.aarch64
    if [[ -r /sys/fs/cgroup/memory.peak ]]; then cat /sys/fs/cgroup/memory.peak > /game/container-memory-peak-bytes.txt; fi
  ' || status=$?
finished=$(date +%s)
printf 'lto_workers=8 memory_limit_gib=64 wall_seconds=%s start_unix=%s end_unix=%s status=%s\n' "$((finished-started))" "$started" "$finished" "$status" >> build/lto-stage/game/link-time.txt
[[ "$status" == 0 ]] || { tail -60 build/lto-stage/game/link.log; exit "$status"; }
readelf -h build/lto-stage/game/desert-storm-lto.aarch64 > build/lto-stage/game/elf-header.txt
readelf -d build/lto-stage/game/desert-storm-lto.aarch64 > build/lto-stage/game/dynamic.txt
readelf -h build/lto-stage/game/desert-storm-lto.aarch64 | rg -q 'Machine:.*AArch64'
sha256sum build/lto-stage/game/desert-storm-lto.aarch64 > build/lto-stage/game/SHA256SUMS
