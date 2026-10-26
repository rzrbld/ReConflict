/* Differential test for the original translated memory-copy entry plus the
 * recovered signed jump-table aliases. Compares memory and cdecl ABI results
 * against native memmove across overlap, non-overlap, counts and alignments. */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "recomp_types.h"
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp;
uint32_t g_cur_func,g_icall_trace[ICALL_TRACE_SIZE],g_icall_from[ICALL_TRACE_SIZE],g_icall_trace_idx,g_icall_count;
ptrdiff_t g_mem_base;
extern void recomp_desertstorm_00568A70(void);
extern recomp_func_t ds_recovered_lookup(uint32_t va);
recomp_func_t recomp_lookup(uint32_t va){if(va==0x568a70)return recomp_desertstorm_00568A70;return ds_recovered_lookup(va);}
recomp_func_t recomp_lookup_import(uint32_t va){(void)va;return NULL;}
recomp_func_t recomp_lookup_manual(uint32_t va){if(va==RECOMP_RETADDR||recomp_lookup(va))return NULL;fprintf(stderr,"Unresolved test dispatch %#x from %#x\n",va,g_cur_func);abort();}
void recomp_dump_trace(const char *why){fprintf(stderr,"%s\n",why);}
int main(void){
    g_mem_base=(ptrdiff_t)calloc(1,0x1000000);if(!g_mem_base)return 1;
    FILE *image=fopen("build/game/memmove-image.bin","rb");if(!image)return 1;
    size_t image_bytes=fread((void*)ADDR(0x400000),1,0x403000,image);fclose(image);if(image_bytes!=0x403000)return 1;
    unsigned char initial[2048],expected[2048];for(unsigned i=0;i<sizeof initial;++i)initial[i]=(unsigned char)((i*31+i/7)^0x5a);
    unsigned cases=0;
    for(unsigned src=0;src<32;++src)for(unsigned dest=0;dest<32;++dest)for(unsigned count=0;count<=128;++count){
        memcpy(expected,initial,sizeof expected);memcpy((void*)ADDR(0x1000),initial,sizeof initial);memmove(expected+dest,expected+src,count);
        g_esp=0xf000;g_ebp=0x1234abcd;g_esi=0x4321aabb;g_edi=0x44aa88bb;g_ebx=0xaabbccdd;g_eax=0;g_ecx=0x77;g_edx=0x55;
        MEM32(g_esp)=RECOMP_RETADDR;MEM32(g_esp+4)=0x1000+dest;MEM32(g_esp+8)=0x1000+src;MEM32(g_esp+12)=count;
        recomp_desertstorm_00568A70();
        if(memcmp(expected,(void*)ADDR(0x1000),sizeof expected)||g_eax!=0x1000+dest||g_esp!=0xf004||g_ebp!=0x1234abcd||g_esi!=0x4321aabb||g_edi!=0x44aa88bb||g_ebx!=0xaabbccdd){fprintf(stderr,"FAIL src=%u dest=%u count=%u eax=%#x esp=%#x\n",src,dest,count,g_eax,g_esp);return 1;}
        ++cases;
    }
    free((void*)g_mem_base);printf("Recovered memmove aliases: %u overlap/alignment/count cases, memory and cdecl registers PASS\n",cases);return 0;
}
