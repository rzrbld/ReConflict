/* Exact bounded original0x595361/0x5953D3 4x4 Jacobian product kernels. */
#include "recomp_types.h"
#include "jinvmjt_fast.h"
#include <limits.h>
#ifdef __FAST_MATH__
#error Jacobian product must preserve operation order
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("fp-contract=off")
#endif
static int enabled(void){static int init,value;if(!init){const char*s=getenv("DS_FAST_MATH");value=s&&!strcmp(s,"1");init=1;}return value;}
static int overlap(uint32_t a,uint32_t n,uint32_t b,uint32_t m){return (uint64_t)a+n>b&&(uint64_t)b+m>a;}
static float load(uint32_t p){float v;memcpy(&v,(void*)ADDR(p),4);return v;}
int ds_jinvmjt_fast_kernel(int accumulate){
 if(!enabled())return 0;
 uint32_t a=g_esi,b=g_ebx,d=g_edi;
 if(!a||!b||!d||(accumulate!=0&&accumulate!=1)||g_eax!=4||g_fp_top>INT_MAX-7||a>UINT32_MAX-96||b>UINT32_MAX-96||d>UINT32_MAX-64)return 0;
 if(overlap(a,96,d,64)||overlap(b,96,d,64))return 0;
 for(unsigned i=0;i<24;i++)if(!isfinite(load(a+i*4))||!isfinite(load(b+i*4)))return 0;
 if(accumulate)for(unsigned i=0;i<16;i++)if(!isfinite(load(d+i*4)))return 0;
 for(unsigned row=0;row<4;row++){
  double s0=0.,s1=0.,s2=0.,s3=0.;
  for(unsigned k=0;k<6;k++){
   double x=load(a+row*4+k*16);
   double p1=(double)load(b+k*16+4)*x;s1=s1+p1;
   double p0=(double)load(b+k*16)*x;s0=s0+p0;
   double p3=(double)load(b+k*16+12)*x;s3=s3+p3;
   double p2=x*(double)load(b+k*16+8);s2=s2+p2;
  }
  if(accumulate){s0=s0+(double)load(d+row*16);s1=s1+(double)load(d+row*16+4);s2=s2+(double)load(d+row*16+8);s3=s3+(double)load(d+row*16+12);}
  float f0=(float)s0,f1=(float)s1,f2=(float)s2,f3=(float)s3;
  memcpy((void*)ADDR(d+row*16),&f0,4);memcpy((void*)ADDR(d+row*16+4),&f1,4);memcpy((void*)ADDR(d+row*16+8),&f2,4);memcpy((void*)ADDR(d+row*16+12),&f3,4);
 }
 g_esi=a+16;g_edi=d+64;g_eax=g_ecx=0;g_edx=96;
 g_flag_k=FK_DEC;g_flag_a=g_flag_b=1;g_flag_cf=0;
 for(unsigned i=1;i<8;i++){g_st[i]=0.;g_st_i64[i]=0;}
 return 1;
}
