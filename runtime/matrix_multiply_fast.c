/* Guarded replacement for original0x4AC1FB, including its lifted ABI/FPU
 * side effects. No generated hook is installed by this translation unit. */
#include "recomp_types.h"
#include "matrix_multiply_fast.h"
#include <limits.h>
#ifdef __FAST_MATH__
#error Guest matrix multiplication must not use fast-math
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize ("fp-contract=off")
#endif
static int enabled(void){
    static int initialized,value;
    if(!initialized){const char *s=getenv("DS_FAST_MATH");value=s&&!strcmp(s,"1");initialized=1;}
    return value;
}
static int overlap(uint32_t a,uint32_t an,uint32_t b,uint32_t bn){
    return (uint64_t)a+an>b&&(uint64_t)b+bn>a;
}
int ds_matrix_multiply_fast_4ac1fb(void){
    if(!enabled())return 0;
    uint32_t s=g_esp;
    if(s<84||s>UINT32_MAX-16u||g_fp_top>INT_MAX-16)return 0;
    uint32_t dst=MEM32(s+4),left=MEM32(s+8),right=MEM32(s+12);
    if(dst>UINT32_MAX-64u||left>UINT32_MAX-64u||right>UINT32_MAX-64u)return 0;
    /* The original writes saved registers before reading matrices. Unsupported
     * stack aliases and partial destination overlaps keep the original order. */
    if(overlap(dst,64,s-84,100)||overlap(left,64,s-84,100)||overlap(right,64,s-84,100))return 0;
    if((dst!=left&&overlap(dst,64,left,64))||(dst!=right&&overlap(dst,64,right,64)))return 0;
    float a[16],b[16],out[16];
    memcpy(a,(void*)ADDR(left),sizeof a);memcpy(b,(void*)ADDR(right),sizeof b);
    for(unsigned i=0;i<16;i++)if(!isfinite(a[i])||!isfinite(b[i]))return 0;
    /* Matches the lifted binary64 model, including its explicit reduction
     * tree. CW is preserved; the reference does not apply CW to C arithmetic.
     * Finite float products cannot overflow binary64. Final stores may overflow
     * float32 exactly as the lifted casts do. No contraction/reassociation. */
    for(unsigned row=0;row<4;row++)for(unsigned col=0;col<4;col++){
        double p0=(double)a[row*4]*(double)b[col];
        double p1=(double)a[row*4+1]*(double)b[4+col];
        double p2=(double)a[row*4+2]*(double)b[8+col];
        double p3=(double)a[row*4+3]*(double)b[12+col];
        double x=p2+p0,y=p1+p3;
        out[row*4+col]=(float)(x+y);
    }
    int same=dst==left&&dst==right,right_only=dst==right&&dst!=left;
    /* Preserve all original stack writes, even below the returned stack. */
    MEM32(s-4)=g_ebp;MEM32(s-72)=g_ebx;MEM32(s-76)=g_esi;MEM32(s-80)=g_edi;
    if(same){MEM32(s-84)=16;memcpy((void*)ADDR(s-68),b,64);MEM32(s+12)=s-68;}
    memcpy((void*)ADDR(dst),out,sizeof out);
    /* FFREE is a no-op in this runtime. Four retained input values remain
     * after each outer iteration, leaving top+16 and the last row/column. */
    for(unsigned i=0;i<4;i++)g_st[i]=right_only?(double)b[15-i*4]:(double)a[15-i];
    for(unsigned i=4;i<8;i++)g_st[i]=0.;
    memset(g_st_i64,0,sizeof g_st_i64);g_fp_top+=16;
    g_eax=dst;g_ecx=right_only?left:left+64;g_edx=same?s-68:right;
    g_flag_k=FK_INC;g_flag_a=UINT32_MAX;g_flag_b=1;
    g_flag_cf=right_only?1u:(uint32_t)(right<dst);
    g_esp=s+16;g_cur_func=0x4ac1fb;
    return 1;
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif
