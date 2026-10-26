#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/audio-tests
python3 - <<'PY'
from pathlib import Path
import re,hashlib
assert hashlib.sha256(Path('setup_conflict_-_desert_storm_1.0_(21657)/DesertStorm.exe').read_bytes()).hexdigest()=='d712f5ec82c14da66b098332b3cc9c99689091f46317112ea35dfd393dc3e164'
s=Path('build/translated/desertstorm/functions_0078.c').read_text();m=re.search(r'^void recomp_desertstorm_00561D50\(void\) \{.*?(?=^void |\Z)',s,re.M|re.S);assert m
s=re.sub(r'^    if\(ds_audio_decode_fast_561d50\(\)\)return;.*\n','',m.group(),flags=re.M).replace('recomp_desertstorm_00561D50','reference_00561D50')
Path('build/audio-tests/reference.c').write_text('#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\n'+s)
PY
"${CC:-cc}" -std=gnu11 -O2 -fno-strict-aliasing -fwrapv -ffp-contract=off -Wno-unused-label -Wno-unused-variable -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 tests/audio_decode_fast.c runtime/audio_decode_fast.c build/audio-tests/reference.c -lm -o build/audio-tests/selftest
env -u DS_FAST_MATH build/audio-tests/selftest --disabled
DS_FAST_MATH=0 build/audio-tests/selftest --disabled
DS_FAST_MATH=1 build/audio-tests/selftest
if [[ ${1:-} == --cross ]]; then
 image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
 docker run --rm --network none --read-only --tmpfs /tmp --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" -v "$PWD/build/audio-tests:/out:rw" --workdir /src "$image" aarch64-linux-gnu-gcc \
 -std=gnu11 -O2 -mcpu=cortex-a53 -fno-strict-aliasing -fwrapv -ffp-contract=off -DRECOMP_FP_STACK_EXACT=1 -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 \
 tests/audio_decode_fast.c runtime/audio_decode_fast.c runtime/fp_stack_exact.c build/audio-tests/reference.c -lm -o /out/audio-decode-fast-selftest.aarch64
elif [[ -n ${1:-} ]]; then
 echo 'Usage: test_audio_decode_fast.sh [--cross]' >&2; exit 1
fi
