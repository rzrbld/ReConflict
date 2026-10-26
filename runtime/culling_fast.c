/* Exact finite prefix replacement for original 0x50C359.
 * No traversal, callback, graph or visibility-policy changes. */
#include "recomp_types.h"
#include "culling_fast.h"
#ifdef __FAST_MATH__
#error Culling equivalence requires strict floating-point arithmetic
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize ("fp-contract=off")
#endif
static int checked,value;
static unsigned long long attempts,accepted,rejected,declined;
void ds_culling_fast_control(int enable){
    fprintf(stderr,"[culling-prefix] previous=%d attempted=%llu accepted_fast=%llu rejected_fast=%llu declined=%llu\n",value,attempts,accepted,rejected,declined);
    checked=1;value=enable!=0;attempts=accepted=rejected=declined=0;
}
static int decline(void){declined++;return 0;}
static int enabled(void){if(!checked){const char*e=getenv("DS_FAST_CULLING");value=e&&!strcmp(e,"1");checked=1;}return value;}
static int overlap(uint32_t a,uint32_t n,uint32_t b,uint32_t m){return (uint64_t)a+n>b&&(uint64_t)b+m>a;}
static float floating(uint32_t a){float v;memcpy(&v,(void*)ADDR(a),4);return v;}
static uint32_t bits(float f){uint32_t b;memcpy(&b,&f,4);return b;}
int ds_culling_prefix_50c359(void){
    if(!enabled())return 0;
    attempts++;
    if(g_esp>UINT32_MAX-44u||g_fp_top>INT32_MAX-2)return decline();
    uint32_t s=g_esp+40,node=g_esi;
    if(s<40||s>UINT32_MAX-4u||node>UINT32_MAX-24u)return decline();
    /* The original writes its prologue before reading these values. All
     * overlap with its stack scratch/saved-register window stays original. */
    if(overlap(node,24,s-40,44)||overlap(0x644024,4,s-40,44)||overlap(0x5dad3c,4,s-40,44)||overlap(0x5d8344,4,s-40,44))return decline();
    uint32_t planes=MEM32(0x644024);
    if(planes>UINT32_MAX-16u)return decline();
    int32_t count=(int32_t)MEM32(planes);
    if(count<1||count>16||planes>UINT32_MAX-16u-(uint32_t)count*16u||overlap(planes,16u+(uint32_t)count*16u,s-40,44))return decline();
    float half=floating(node+8),cx=floating(node+12),cy=floating(node+16),cz=floating(node+20),factor=floating(0x5dad3c),zero=floating(0x5d8344);
    if(!isfinite(half)||!isfinite(cx)||!isfinite(cy)||!isfinite(cz)||!isfinite(factor)||zero!=0.)return decline();
    double product=(double)half*(double)factor;float radius=(float)product;
    if(!isfinite(radius))return decline();
    uint32_t coefficients[4]={0};uint32_t absolute=0,intersects=0,last_sign=0,last_status=0;int32_t rejected_plane=-1;
    for(int32_t i=0;i<count;++i){
        uint32_t p=planes+16+(uint32_t)i*16;
        float c[4];
        for(unsigned j=0;j<4;++j){coefficients[j]=MEM32(p+j*4);memcpy(&c[j],&coefficients[j],4);if(!isfinite(c[j]))return decline();}
        double distance=(double)c[2]*(double)cz;
        double term=(double)c[1]*(double)cy;distance=distance+term;
        term=(double)c[0]*(double)cx;distance=distance+term;
        distance=distance+(double)c[3];
        /* Sign compares the unrounded binary64 dot. Magnitude then compares
         * the original binary32 store with its sign bit cleared. */
        absolute=bits((float)distance)&0x7fffffffu;float magnitude;memcpy(&magnitude,&absolute,4);
        last_sign=distance<0.;
        last_status=magnitude<radius?0x100u:magnitude==radius?0x4000u:0;
        if(last_sign&&(double)magnitude>(double)radius){rejected_plane=i;break;}
        if(magnitude<=radius)intersects=1;
    }
    /* Commit only after proving original rejection. Scratch holds only the
     * final tested plane, because previous iterations overwrite every slot. */
    MEM32(s-24)=absolute;MEM32(s-20)=bits(radius);
    for(unsigned j=0;j<4;++j)MEM32(s-16+j*4)=coefficients[j];
    g_ecx=(coefficients[2]&0xffffff00u)|last_sign;
    g_ebx=(uint32_t)count;g_ebp=intersects;
    g_st[6]=g_st[7]=0.;g_st_i64[6]=g_st_i64[7]=0;
    g_flag_cf=0;
    if(rejected_plane>=0){
        g_eax=0;g_edx=(uint32_t)rejected_plane;g_edi=planes+16+(uint32_t)rejected_plane*16;
        g_flag_k=FK_NARROW(FK_TEST,24);g_flag_a=g_flag_b=1u<<24;
        rejected++;return 2;
    }
    g_eax=last_status;g_edx=(uint32_t)count;g_edi=planes+16+(uint32_t)count*16;
    g_flag_k=FK_CMP;g_flag_a=g_flag_b=(uint32_t)count;
    accepted++;return 1;
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif
