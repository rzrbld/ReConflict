#!/bin/bash
# Staged-only guard variant probes. No game build, deployment or device access.
set -euo pipefail
cd "$(dirname "$0")/.."
proxy=${DS_RT_PROXY_SOURCE:-build/render-thread-stage/client-reset/render_thread_proxy.c}
output=${DS_RT_TEST_OUTPUT:-build/render-thread-stage/client-reset/arm}
expected=${DS_RT_PROXY_SHA256:-c44256061bf8f1c4ed048ae961903267529b990b1aba26a3275cf53fcb6bc5ba}
[[ $(sha256sum "$proxy" | cut -d' ' -f1) == "$expected" ]] || { echo 'Unexpected proxy source SHA' >&2;exit 1; }
mkdir -p "$output"
DS_RT_TEST_OUTPUT="$output/parity" python3 scripts/stage_render_worker_parity.py
python3 - "$proxy" "$output" <<'PY'
from pathlib import Path
import hashlib,json,sys
proxy=Path(sys.argv[1]);out=Path(sys.argv[2]);inputs=[proxy,*sorted(proxy.parent.glob('*.inc')),proxy.parent/'render_thread_proxy.h',Path('probe/render_worker_guard_reset.c'),Path('probe/render_worker_regression.c'),Path('tests/render_worker_fixture.h'),out/'parity/gpu_proxy_selftest.c',out/'parity/parity-lights.c',Path('build/render-thread-game/src/d3d8_renderer.c'),Path('build/render-thread-game/snapshot/runtime/d3d8_renderer.h')]
(out/'inputs.json').write_text(json.dumps({str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},indent=2)+'\n')
PY
image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" \
 -e DS_TEST_PROXY="$proxy" -e DS_PARITY_SOURCE="$output/parity" -v "$PWD:/src:ro" -v "$PWD/$output:/out:rw" -w /src "$image" bash -eu -o pipefail -c '
 flags="-std=gnu11 -O2 -Wall -Wextra -Werror -Iprobe -Ibuild/render-thread-stage -Ibuild/sdk-include -Ibuild/sdk-include/SDL2 -I/usr/include/SDL2"
 aarch64-linux-gnu-gcc $flags -c "$DS_TEST_PROXY" -o /out/proxy.o
 mapfile -t names < <(aarch64-linux-gnu-nm -g --defined-only /out/proxy.o | awk '\''$3 ~ /^__wrap_[A-Za-z_][A-Za-z0-9_]*$/ {sub(/^__wrap_/,"",$3);print $3}'\'' | sort -u)
 wrappers=();for name in "${names[@]}";do wrappers+=("-Wl,--wrap=$name");done
 aarch64-linux-gnu-gcc $flags probe/render_worker_guard_reset.c /out/proxy.o "${wrappers[@]}" -Lbuild/target-libs -Wl,--allow-shlib-undefined -lSDL2 -lGLESv2 -pthread -lm -o /out/guard-reset.aarch64
 aarch64-linux-gnu-gcc $flags probe/render_worker_regression.c /out/proxy.o "${wrappers[@]}" -Lbuild/target-libs -Wl,--allow-shlib-undefined -lSDL2 -lGLESv2 -pthread -lm -o /out/queue-regression.aarch64
 aarch64-linux-gnu-gcc -std=gnu11 -O2 -fno-strict-aliasing -ffp-contract=off \
  -Ibuild/render-thread-game/snapshot/runtime -Iprobe -Ibuild/render-thread-stage -Ibuild/sdk-include -Ibuild/sdk-include/SDL2 -I/usr/include/SDL2 \
  "$DS_PARITY_SOURCE/gpu_proxy_selftest.c" build/render-thread-game/src/d3d8_renderer.c "$DS_PARITY_SOURCE/parity-lights.c" /out/proxy.o "${wrappers[@]}" \
  -Lbuild/target-libs -Wl,--allow-shlib-undefined -lSDL2 -lGLESv2 -pthread -lm -o /out/gpu-parity.aarch64
 '
python3 - "$output" <<'PY'
from pathlib import Path
import hashlib,json,sys
manifest=json.loads((Path(sys.argv[1])/'inputs.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==sha for p,sha in manifest.items()),'Probe sources changed during compilation'
print('Verified source inputs unchanged during isolated ARM build')
PY
sha256sum "$output"/*.aarch64
