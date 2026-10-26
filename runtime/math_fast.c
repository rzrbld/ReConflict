/* Optional bounded fast paths for the original full-matrix vector transforms.
 * Eliminates emulated x87 stack shuffling; arithmetic follows original order.
 * Unsupported matrix modes/nonfinite inputs/stack aliases retain lifted code. */
#include "recomp_types.h"
#include "math_fast.h"
#ifdef __FAST_MATH__
#error Guest math fast paths must not be compiled with fast-math
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize ("fp-contract=off")
#endif
static int enabled(void){static int initialized,value;if(!initialized){const char*e=getenv("DS_FAST_MATH");value=e&&!strcmp(e,"1");initialized=1;}return value;}
static int overlap(uint32_t a,uint32_t an,uint32_t b,uint32_t bn){return (uint64_t)a+an>b&&(uint64_t)b+bn>a;}
static float load_float(uint32_t p){float v;memcpy(&v,(void*)ADDR(p),4);return v;}
static uint32_t float_bits(float v){uint32_t u;memcpy(&u,&v,4);return u;}
static int transform(int point){
    if(!enabled())return 0;
    uint32_t matrix=g_ecx,s=g_esp;
    if(s<12||s>UINT32_MAX-12u||matrix>UINT32_MAX-68u)return 0;
    uint32_t mode=MEM32(matrix+64);
    if(!(mode&2u)||(point&&mode==1))return 0;
    uint32_t src=MEM32(s+4),dst=MEM32(s+8);
    if(src>UINT32_MAX-12u||dst>UINT32_MAX-12u)return 0;
    /* Original writes scratch between repeated source/matrix reads. Keep
     * every stack-alias case in the original, including output/arguments. */
    if(overlap(src,12,s-12,24)||overlap(dst,12,s-12,24)||overlap(matrix,68,s-12,24))return 0;
    float m[16],v[3];
    for(int i=0;i<16;++i){m[i]=load_float(matrix+4u*i);if(!isfinite(m[i]))return 0;}
    for(int i=0;i<3;++i){v[i]=load_float(src+4u*i);if(!isfinite(v[i]))return 0;}
    /* Separate products and additions plus disabled contraction retain each
     * binary64 rounding step. Do not reorder x/y/z terms across rows. */
    double x=(double)v[2]*(double)m[8];
    double term=(double)v[1]*(double)m[4];x=x+term;
    term=(double)v[0]*(double)m[0];x=x+term;
    if(point)x=x+(double)m[12];
    double y=(double)v[0]*(double)m[1];
    term=(double)v[2]*(double)m[9];y=y+term;
    term=(double)v[1]*(double)m[5];y=y+term;
    if(point)y=y+(double)m[13];
    double z=(double)v[0]*(double)m[2];
    term=(double)v[2]*(double)m[10];z=z+term;
    term=(double)v[1]*(double)m[6];z=z+term;
    if(point)z=z+(double)m[14];
    uint32_t bx=float_bits((float)x),by=float_bits((float)y),bz=float_bits((float)z);
    MEM32(s-12)=bx;MEM32(s-8)=by;MEM32(s-4)=bz;
    MEM32(dst)=bx;MEM32(dst+4)=by;MEM32(dst+8)=bz;
    /* Original's deepest x87 push is two slots. Even inactive slots are
     * observable to the lifted runtime; preserve its exact shadow effects. */
    g_st[6]=g_st[7]=0.;g_st_i64[6]=g_st_i64[7]=0;
    if(point){g_eax=by;g_ecx=dst;g_edx=bz;g_cur_func=0x540470;}
    else{g_eax=dst;g_ecx=bz;g_edx=by;g_cur_func=0x5405a0;}
    g_flag_k=FK_ADD;g_flag_a=s-12;g_flag_b=12;g_flag_cf=0;g_esp=s+12;
    return 1;
}
int ds_math_fast_540470(void){return transform(1);}
int ds_math_fast_5405a0(void){return transform(0);}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif
