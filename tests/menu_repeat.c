/* Execute the unchanged translated original MENU_DOWN callback with a controlled
 * game clock. This tests actual repeat behavior, not a copied implementation. */
#include "recomp_types.h"
#include <assert.h>
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func;
ptrdiff_t g_mem_base;
extern void recomp_desertstorm_00478530(void);
static unsigned up_at(unsigned now){
    MEM32(0x2010)=now;g_esp=0xf000;MEM32(g_esp)=RECOMP_RETADDR;MEM32(g_esp+4)=0;
    recomp_desertstorm_00478530();assert(g_esp==0xf004);return g_eax;
}
int main(void){
    g_mem_base=(ptrdiff_t)calloc(1,0x800000);assert(g_mem_base);
    MEM32(0x63c958)=0x2000;MEM32(0x60f734)=4;
    assert(up_at(1000)==1);assert(MEM32(0x60fa14)==1349);
    assert(up_at(1100)==0);assert(up_at(1349)==0);assert(up_at(1350)==1);
    assert(MEM32(0x60fa14)==1475);assert(up_at(1475)==0);assert(up_at(1476)==1);
    MEM32(0x60f734)=0;assert(up_at(1500)==0);assert(MEM8(0x60fa19)==0);
    MEM32(0x60f734)=4;assert(up_at(1501)==1);
    free((void*)g_mem_base);puts("Original MENU_DOWN: immediate step, 349ms initial delay, 125ms repeats, release resets PASS");return 0;
}
