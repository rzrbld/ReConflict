#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/skinning-tests
python3 - <<'PY'
from pathlib import Path
import re,hashlib
p=Path('setup_conflict_-_desert_storm_1.0_(21657)/DesertStorm.exe')
assert hashlib.sha256(p.read_bytes()).hexdigest()=='d712f5ec82c14da66b098332b3cc9c99689091f46317112ea35dfd393dc3e164'
out='#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\nvoid recomp_desertstorm_00540470(void);\nvoid recomp_desertstorm_005405A0(void);\n'
for unit,address in [('0066',0x529520),('0072',0x540470),('0072',0x5405a0)]:
 s=Path(f'build/translated/desertstorm/functions_{unit}.c').read_text();name=f'recomp_desertstorm_{address:08X}'
 m=re.search(r'^void '+name+r'\(void\) \{.*?(?=^void |\Z)',s,re.M|re.S);assert m,name
 body=re.sub(r'^    if\(ds_math_fast_.*?\n','',m.group(),flags=re.M)
 body=re.sub(r'^    if\(ds_skinning_fast_529520\(\)\)return;.*\n','',body,flags=re.M)
 if address==0x529520:body=body.replace(name,'reference_00529520')
 out+=body+'\n'
Path('build/skinning-tests/reference.c').write_text(out)
skin=re.search(r'^void recomp_desertstorm_00529520\(void\) \{.*?(?=^void |\Z)',Path('build/translated/desertstorm/functions_0066.c').read_text(),re.M|re.S).group()
skin=re.sub(r'^    if\(ds_skinning_fast_529520\(\)\)return;.*\n','',skin,flags=re.M)
skin=skin.replace('recomp_desertstorm_00529520','current_00529520').replace('recomp_desertstorm_00540470','current_point').replace('recomp_desertstorm_005405A0','current_vector')
current='#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\n#include "math_fast.h"\nvoid recomp_desertstorm_00540470(void);void recomp_desertstorm_005405A0(void);\n'
current+='static void current_point(void){if(!ds_math_fast_540470())recomp_desertstorm_00540470();}\nstatic void current_vector(void){if(!ds_math_fast_5405a0())recomp_desertstorm_005405A0();}\n'+skin
Path('build/skinning-tests/current.c').write_text(current)
PY
"${CC:-cc}" -std=gnu11 -O2 -fno-strict-aliasing -fwrapv -ffp-contract=off \
 -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 tests/skinning_fast.c runtime/skinning_fast.c \
 runtime/math_fast.c build/skinning-tests/reference.c build/skinning-tests/current.c -lm -o build/skinning-tests/skinning-fast-selftest
DS_FAST_MATH=0 build/skinning-tests/skinning-fast-selftest --disabled
DS_FAST_MATH=1 build/skinning-tests/skinning-fast-selftest
if [[ ${1:-} == --cross ]]; then
 image=${CROSS_IMAGE:-sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49}
 docker run --rm --network none --read-only --tmpfs /tmp \
  --user "$(id -u):$(id -g)" -v "$PWD:/src:ro" -v "$PWD/build/skinning-tests:/out:rw" \
  --workdir /src "$image" aarch64-linux-gnu-gcc \
  -std=gnu11 -O2 -mcpu=cortex-a53 -fno-strict-aliasing -fwrapv -ffp-contract=off \
  -DRECOMP_FP_STACK_EXACT=1 -Iruntime -Ithird_party/pcrecomp/runtime/recomp32 \
  tests/skinning_fast.c runtime/skinning_fast.c runtime/math_fast.c runtime/fp_stack_exact.c \
  build/skinning-tests/reference.c build/skinning-tests/current.c -lm \
  -o /out/skinning-fast-selftest.aarch64
elif [[ -n ${1:-} ]]; then
 echo 'Usage: test_skinning_fast.sh [--cross]' >&2
 exit 1
fi
