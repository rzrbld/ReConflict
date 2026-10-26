#!/usr/bin/env python3
"""Stage ordered scalarization of 595DAA–595DEF; preserve the frozen build."""
from pathlib import Path
import hashlib
import json
import re
import subprocess
from physics_fast_spec import validate_binary, extract, original_body

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/physics-product'
OUT.mkdir(parents=True, exist_ok=True)
validate_binary()
source = (ROOT / 'build/translated/desertstorm/functions_0097.c').read_text()
assert hashlib.sha256(source.encode()).hexdigest() == '5d51050aba1ccbb08b44b5fb320aa511e926fde3d09a1e34a79899202951d9b6'
archive = ROOT / 'build/render-thread-game/snapshot/build/translated/desertstorm/lifted.aarch64.a'
obj = subprocess.check_output(['ar', 'p', str(archive), 'functions_0097.aarch64.o'])
assert hashlib.sha256(obj).hexdigest() == 'a5b729adca1ed51383928c674dd1f25848bb47336bed986d07dbdff4a85e1804'
assert obj == (ROOT / 'build/translated/desertstorm/functions_0097.aarch64.o').read_bytes()
end = '    _flag_a = (uint32_t)(esi); _flag_b = (uint32_t)(4);'
regions = []
for va in ('00595B10', '00595B59'):
    f = original_body(extract(source, va), va)
    start = f.index('L_00595DAA:')
    regions.append(f[start:f.index(end, start)])
assert regions[0] == regions[1]
region = regions[0]
assert hashlib.sha256(region.encode()).hexdigest() == 'd9124509e76ef6d44d6fee089be919d64c469626e671b368deac7203e053b657'
prefix = '#define RECOMP_GENERATED_CODE\n#include "recomp_types.h"\n'
flags = '    uint32_t _cf=0, _flag_a=0, _flag_b=0, _flag_k=0; RECOMP_FLAGS_IN();\n'
(OUT / 'reference.c').write_text(prefix + 'void reference_product(void) {\n' + flags + region + '    RECOMP_FLAGS_OUT();\n}\n')
# Unroll exactly the original four iterations. Each arithmetic statement stays
# binary64 and in the original order; float writes occur only after all reads.
head, loop = region.split('L_00595DBD:\n')
loop, tail = loop.split('L_00595DE3:\n')
lines = loop.splitlines()
assert sum('goto ' in x for x in lines) == 1
assert 'goto L_00595DBD;' in lines[-2] or 'goto L_00595DBD;' in lines[-1]
loop = '\n'.join(x for x in lines if 'goto L_00595DBD;' not in x)
unrolled = head + '\n' + (loop + '\n') * 4 + tail
stack = [f'g_st[{i}]' for i in range(8)]
shadow = [f'g_st_i64[{i}]' for i in range(8)]
depth = peak = count = 0
out = []

def subst(s):
    return re.sub(r'_st\[(\d)\]', lambda m: stack[int(m[1])], s)

def new(expr):
    global count
    count += 1
    name = f'v{count}'
    out.append(f'    double {name} = {expr};')
    return name

def pop():
    global depth
    value = stack.pop(0)
    stack.append('0.0')
    shadow.pop(0)
    shadow.append('0')
    depth -= 1
    assert depth >= 0
    return value

for line in unrolled.splitlines():
    code = line.split('/*')[0].strip()
    if not code or code.startswith('L_'):
        continue
    assert 'goto ' not in line
    if code.startswith('fp_push('):
        v = new(subst(code[len('fp_push('):-2]))
        stack = [v] + stack[:7]
        shadow = ['0'] + shadow[:7]
        depth += 1
        peak = max(peak, depth)
    elif code.startswith('fp_xch('):
        i = int(re.search(r'\((\d)\)', code)[1])
        stack[0], stack[i] = stack[i], stack[0]
        shadow[0], shadow[i] = shadow[i], shadow[0]
    elif code.startswith('{ double _v = fp_pop();'):
        v = pop()
        m = re.fullmatch(r'\{ double _v = fp_pop\(\); _st\[(\d)\] \+= _v; \}', code)
        assert m, code
        i = int(m[1])
        stack[i] = new(stack[i] + ' + ' + v)
    elif code.startswith('{ float _v = (float)fp_pop();'):
        v = pop()
        out.append('    ' + code.replace('fp_pop()', v))
    elif code.startswith('_st['):
        m = re.fullmatch(r'_st\[(\d)\] \*= (.*);', code)
        assert m, code
        i = int(m[1])
        stack[i] = new(stack[i] + ' * (' + subst(m[2]) + ')')
    else:
        assert '_st[' not in code and 'fp_' not in code, code
        out.append(line)
assert depth == 0 and peak == 6
assert stack == ['g_st[0]', 'g_st[1]'] + ['0.0'] * 6
assert shadow == ['g_st_i64[0]', 'g_st_i64[1]'] + ['0'] * 6
out += ['    memset(g_st+2, 0, 6*sizeof(*g_st));',
        '    memset(g_st_i64+2, 0, 6*sizeof(*g_st_i64));']
fragment = '/* Generated ordered physics matrix product; no arithmetic reassociation. */\n' + '\n'.join(out) + '\n'
(OUT / 'physics_product.inc').write_text(fragment)
(OUT / 'physics_product.h').write_text('''#ifndef DS_PHYSICS_PRODUCT_H
#define DS_PHYSICS_PRODUCT_H
int ds_physics_product_eligible(void);
#endif
''')
(OUT / 'physics_product.c').write_text('''#include "recomp_types.h"
#include "physics_product.h"
#include <limits.h>
#ifdef __FAST_MATH__
#error Physics guards require finite input checks
#endif
int ds_physics_product_eligible(void) {
    static int initialized, enabled;
    if (!initialized) {
        const char *s=getenv("DS_FAST_PHYSICS_PRODUCT");
        enabled=s&&!strcmp(s,"1"); initialized=1;
    }
    if (!enabled) return 0;
    if (g_ebp<12 || g_ebp>UINT32_MAX-4 || g_fp_top>INT_MAX-6) return 0;
    uint64_t index=MEM32(g_ebp-12);
    uint64_t a=(uint64_t)g_esi+index*4, b=(uint64_t)g_edi+index*4;
    if (!a || !b || !g_ebx || a+52>UINT32_MAX || b+64>UINT32_MAX ||
        (uint64_t)g_ebx+16>UINT32_MAX || index+16>UINT32_MAX) return 0;
    for (unsigned i=0;i<4;i++) {
        if (!isfinite(*(float*)ADDR((uint32_t)a+i*16)) ||
            !isfinite(*(float*)ADDR(g_ebx+i*4))) return 0;
    }
    for (unsigned i=0;i<16;i++)
        if (!isfinite(*(float*)ADDR((uint32_t)b+i*4))) return 0;
    return 1;
}
''')
(OUT / 'kernel.c').write_text(prefix + '#include "physics_product.h"\nint native_product(void) {\n    if (!ds_physics_product_eligible()) return 0;\n' + flags + '#include "physics_product.inc"\n    RECOMP_FLAGS_OUT(); return 1;\n}\n')
hook = '''    if (ds_physics_product_eligible()) {
#include "physics_product.inc"
        goto L_DS_PHYSICS_PRODUCT_DONE;
    }
'''
def patch(s):
    replacement = region.replace('L_00595DAA:\n', 'L_00595DAA:\n' + hook, 1)
    for va in ('00595B10', '00595B59'):
        body = re.search(r'^void (?:recomp_desertstorm_|native_)' + va + r'\(void\) \{.*?(?=^void |\Z)', s, re.M | re.S).group()
        assert body.count(region) == 1
        s = s.replace(body, body.replace(region, replacement + 'L_DS_PHYSICS_PRODUCT_DONE:\n'), 1)
    anchor = '#include "functions.h"' if '#include "functions.h"' in s else '#include "recomp_types.h"'
    return s.replace(anchor, anchor + '\n#include "physics_product.h"', 1)

(OUT / 'functions_0097.c').write_text(patch(source))
(OUT / 'functions.h').write_bytes((ROOT / 'build/translated/desertstorm/functions.h').read_bytes())
rank = (ROOT / 'runtime/physics_rank_update.inc').read_bytes()
assert hashlib.sha256(rank).hexdigest() == '07abd3e520d608043f591baf4fe73aac22aaec37547eb78179eb92cb6a07bd9c'
(OUT / 'physics_rank_update.inc').write_bytes(rank)
# Reuse complete factorization/continuation fixtures and their unchanged guest
# profiling callees. These generated test copies never modify the game sources.
subprocess.run(['python3', str(ROOT / 'scripts/prepare_physics_tests.py')], check=True)
stage = ROOT / 'build/physics-stage'
full = patch((stage / 'full-native.c').read_text())
full = full.replace('if (ds_physics_product_eligible()) {', 'if (ds_physics_product_eligible()) { ++ds_product_accepted;')
full = 'extern unsigned ds_product_accepted;\n' + full
(OUT / 'full-native.c').write_text(full)
for name in ('full-reference.c', 'precise-callees.c'):
    (OUT / name).write_bytes((stage / name).read_bytes())
test = (ROOT / 'tests/physics_fast.c').read_text()
test = test.replace('#include <assert.h>', '#include <assert.h>\n#include <fenv.h>\nunsigned ds_product_accepted;')
test = test.replace('n<240', 'n<960').replace('memset((void*)g_mem_base,0,bytes);unsigned blocks', 'assert(!fesetround((int[]){FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}[n/240]));\n  memset((void*)g_mem_base,0,bytes);unsigned blocks')
test = test.replace('free(initial);', 'printf("product accepted=%u\\n", ds_product_accepted); assert((getenv("DS_FAST_PHYSICS_PRODUCT") && !strcmp(getenv("DS_FAST_PHYSICS_PRODUCT"),"1")) ? ds_product_accepted>0 : ds_product_accepted==0); free(initial);')
(OUT / 'full-test.c').write_text(test)
(OUT / 'manifest.json').write_text(json.dumps({
    'region_sha256': hashlib.sha256(region.encode()).hexdigest(),
    'unit_sha256': hashlib.sha256(source.encode()).hexdigest(),
    'frozen_object_sha256': hashlib.sha256(obj).hexdigest(),
    'scalar_values': count, 'max_depth': peak, 'guest_region': ['0x595DAA', '0x595DEF'],
    'enable': 'DS_FAST_PHYSICS_PRODUCT=1',
    'limits': 'Finite operands and bounded addresses; original fallback otherwise. No FPS claim.'
}, indent=2) + '\n')
print(f'Staged physics product: {count} ordered scalar values, peak depth {peak}')
