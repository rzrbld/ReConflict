#!/usr/bin/env python3
"""Fail-closed and lexical tests for the staging-only local-register pass."""
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from stage_local_regs import transform

def must_reject(source):
    try:transform(source)
    except ValueError:return
    raise AssertionError('Unexpectedly accepted '+source)

body='''void recomp_example(void) {
    /* return; } RECOMP_ENTER(fake); */
    uint32_t _flag_k=0;
    RECOMP_ENTER(0x1234);
    if(eax) return;
    puts("return; }");
    { eax=3; }
}'''
out,report=transform(body)
assert 'if(eax) { RECOMP_REGS_OUT(); return; }' in out
assert 'puts("return; }");' in out and '/* return; } RECOMP_ENTER(fake); */' in out
assert out.count('RECOMP_REGS_LOCALS')==1 and out.count('checked fallthrough flush')==1 and report[0]['returns']==1
must_reject(out)
hook='''void recomp_hook(void) {
 if(ds_math_fast_540470())return;
 uint32_t _flag_k=0;
 RECOMP_ENTER(0x1234);eax=1;return;
}'''
out,report=transform(hook)
assert out.index('if(ds_math_fast_540470())return;')<out.index('RECOMP_REGS_LOCALS')
assert report[0]['early_native_hooks']==['ds_math_fast_540470']
must_reject(hook.replace('ds_math_fast_540470','unreviewed_hook'))
must_reject(hook.replace('uint32_t _flag_k=0;','eax=1;'))
must_reject(body.replace('if(eax) return;','return 3;'))
must_reject('void recomp_bad(void){eax=1;return;}')
out,report=transform('void recomp_native(void){g_cur_func=3;ds_guest_longjmp();}')
assert report==[{'name':'recomp_native','native_only':True}]
print('PASS lexical comments/strings, conditional return blocks, fallthrough, native hooks, native-only bodies, and fail-closed rejection')
