#include "win32hle.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned guards,calls;
static void guest(void){calls+=10;}
static void native(void){calls++;}
const recomp_dispatch_entry_t recomp_dispatch_table[]={{0x401000,guest}};
const uint32_t recomp_dispatch_count=1;
void ds_guard_dispatch(uint32_t va){(void)va;guards++;}
int main(void){
    win32hle_shim shims[]={{"first",native},{"last",native},{NULL,NULL}};
    assert(win32hle_register(shims)==2);
    uint32_t a=hle_resolve("first"),b=hle_resolve("last");
    assert(a&&b&&b-a==16);
    assert(!strcmp(hle_name(a),"first")&&!strcmp(hle_name(b),"last"));
    assert(!hle_name(a-1)&&!hle_name(a+1)&&!hle_name(b+16)&&!hle_name(0xffffffffu));
    recomp_func_t f=recomp_lookup_manual(a);assert(f);f();
    f=recomp_lookup_manual(b);assert(f);f();
    f=recomp_lookup_manual(0x401000);assert(f==guest);f();
    assert(calls==12&&guards==0);
    assert(!recomp_lookup_manual(0x401001)&&guards==1);
    assert(!recomp_lookup_manual(a+1)&&guards==2);
    assert(!recomp_lookup_manual(RECOMP_RETADDR)&&guards==3);
    puts("PASS strict dispatch: imports, guest functions, name bounds and unknown-target guard");
    return 0;
}
