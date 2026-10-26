/* The original joystick binding evaluator, logical action update, and menu
 * callback must accept a single pressed sample (flags 6 includes edge bit 2). */
#include "recomp_types.h"
#include <assert.h>
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func;
ptrdiff_t g_mem_base;
uint32_t g_icall_trace[ICALL_TRACE_SIZE],g_icall_from[ICALL_TRACE_SIZE],g_icall_trace_idx,g_icall_count;
extern void recomp_desertstorm_004508F0(void),recomp_desertstorm_0044EFB0(void),recomp_desertstorm_0044F000(void),recomp_desertstorm_00478440(void);
recomp_func_t recomp_lookup(uint32_t a){fprintf(stderr,"Unexpected dispatch %#x\n",a);abort();}
recomp_func_t recomp_lookup_manual(uint32_t a){(void)a;return NULL;}
recomp_func_t recomp_lookup_import(uint32_t a){(void)a;return NULL;}
void recomp_dump_trace(const char*s){fprintf(stderr,"%s\n",s);}
static unsigned call(recomp_func_t f,unsigned argument){g_ecx=0x60f5b8;g_esp=0xf000;MEM32(g_esp)=RECOMP_RETADDR;MEM32(g_esp+4)=argument;f();return g_eax;}
int main(void){
    g_mem_base=(ptrdiff_t)calloc(1,0x1000000);assert(g_mem_base);FILE*f=fopen("build/game/memmove-image.bin","rb");assert(f);assert(fread((void*)ADDR(0x400000),1,0x403000,f)==0x403000);fclose(f);
    MEM32(0x60687c)=0x3000;MEM32(0x3000)=1;MEM32(0x3004)=3;MEM32(0x3008)=80;MEM32(0x300c)=0x4000;memset((void*)ADDR(0x4000),0xff,80*3*4);MEM32(0x4000+(80+66)*4)=0x20002;
    MEM32(0x60f5f0)=3;MEM32(0x60f980)=0;MEM32(0x617c18)=1;MEM16(0x610798)=1;MEM32(0x60f744)=1;
    const unsigned raw[]={6,4,9,1,6,9};const unsigned edge[]={1,0,0,0,1,0};
    for(unsigned i=0;i<6;++i){MEM32(0x7541c0)=raw[i];unsigned held=call(recomp_desertstorm_004508F0,66);assert(held==((raw[i]&4)!=0));call(held?recomp_desertstorm_0044EFB0:recomp_desertstorm_0044F000,66);assert(call(recomp_desertstorm_00478440,0)==edge[i]);}
    free((void*)g_mem_base);puts("Original MENU_FORWARD: one pressed sample maps joystick button2 to action66 and exactly one accept edge PASS");return 0;
}
