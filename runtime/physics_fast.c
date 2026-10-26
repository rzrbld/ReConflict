#include "recomp_types.h"
#include "physics_fast.h"
#ifdef __FAST_MATH__
#error Physics guards require finite input checks
#endif
static int enabled(void){
    static int initialized,value;
    if(!initialized){const char *s=getenv("DS_FAST_MATH");value=s&&!strcmp(s,"1");initialized=1;}
    return value;
}
int ds_physics_rank_update_eligible(void){
    if(!enabled())return 0;
    if(g_ecx!=4||g_edx>UINT32_MAX/4)return 0;
    uint64_t source=(uint64_t)g_esi+(uint64_t)g_edx*4,destination=g_ebx;
    if(!source||!destination||source+64>UINT32_MAX||destination+64>UINT32_MAX)return 0;
    if(source+64>destination&&destination+64>source)return 0;
    for(unsigned i=0;i<16;++i){
        if(!isfinite(*(float*)ADDR((uint32_t)source+4*i))||
           !isfinite(*(float*)ADDR((uint32_t)destination+4*i)))return 0;
    }
    return 1;
}
