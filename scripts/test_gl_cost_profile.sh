#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=$(mktemp -d /tmp/ds-gl-cost.XXXXXX)
trap 'rm -rf "$out"' EXIT
read -r -a wraps <<< "$(python3 scripts/gl_cost_flags.py)"
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -Iprobe \
    probe/gl_cost_profile.c tests/gl_cost_profile.c tests/gl_cost_calls.c \
    "${wraps[@]}" -lGLESv2 -o "$out/test"
DS_ROOT="$out" DS_GL_COST=1 DS_GL_COST_FRAMES=2 "$out/test"
python3 - "$out/gl-cost.csv" "$out/first.csv" <<'PY'
import csv,sys
with open(sys.argv[1]) as f:
    header=next(f)
    assert '1 complete frames' in header and 'no subtraction' in header,header
    rows=list(csv.DictReader(f))
assert {r['api'] for r in rows}=={'glDrawArrays','glBindBuffer','glGetError'},rows
assert all(int(r['calls'])==1 and int(r['frame'])==1 for r in rows),rows
assert all(int(r['frame_ns'])>=int(r['total_ns'])>=int(r['max_ns'])>=0 for r in rows),rows
with open(sys.argv[2]) as f:
    assert '2 complete frames' in next(f)
    rows=list(csv.DictReader(f))
assert len(rows)==6,rows
assert {r['api'] for r in rows}=={'glDrawArrays','glBindBuffer','glGetError'},rows
assert all(int(r['calls'])==int(r['frame']) for r in rows),rows
print('GL cost CSV exact API counts/bounds PASS')
PY
