/* Exact lifted-model ADPCM predictor, original 0x561D50. */
#include "recomp_types.h"
#include "audio_decode_fast.h"
#include <limits.h>
#ifdef __FAST_MATH__
#error ADPCM history requires ordered arithmetic
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("fp-contract=off")
#endif
static int enabled(void){static int init,value;if(!init){const char*s=getenv("DS_FAST_MATH");value=s&&!strcmp(s,"1");init=1;}return value;}
static int overlaps(uint32_t a,uint32_t n,uint32_t b,uint32_t m){return (uint64_t)a+n>b&&(uint64_t)b+m>a;}
int ds_audio_decode_fast_561d50(void){
 if(!enabled())return 0;
 uint32_t s=g_esp,t=g_ecx;
 if(s<32||s>UINT32_MAX-20||t>UINT32_MAX-28||g_fp_top>INT_MAX-4)return 0;
 if(overlaps(s-32,52,0x5fe64c,40)||overlaps(s-32,52,0x5d843c,4))return 0;
 uint32_t in=MEM32(s+4),out=MEM32(s+8),n=MEM32(s+12),stereo=MEM8(s+16),stride=stereo?4:2;
 if(!n||n>4096||in>UINT32_MAX-n*2||out>UINT32_MAX-n*stride)return 0;
 uint32_t idx=MEM32(t+20),shift=MEM32(t+24);
 if(idx>=5||shift>15)return 0;
 if(overlaps(in,n*2,out,n*stride)||overlaps(in,n*2,t,28)||overlaps(out,n*stride,t,28)||overlaps(in,n*2,s-32,52)||overlaps(out,n*stride,s-32,52)||overlaps(t,28,s-32,52))return 0;
 float h1,h2,c0,c1,one;memcpy(&h1,(void*)ADDR(t+4),4);memcpy(&h2,(void*)ADDR(t+8),4);
 memcpy(&c0,(void*)ADDR(0x5fe64c+idx*8),4);memcpy(&c1,(void*)ADDR(0x5fe650+idx*8),4);memcpy(&one,(void*)ADDR(0x5d843c),4);
 if(!isfinite(h1)||!isfinite(h2)||!isfinite(c0)||!isfinite(c1)||one!=1.f)return 0;
 float scale=(float)((double)one/(double)(int32_t)(1u<<shift));int16_t pcm[4096];uint32_t last_integer=0;float last=0;
 for(uint32_t i=0;i<n;i++){
  int16_t sample;memcpy(&sample,(void*)ADDR(in+i*2),2);
  double x=(double)sample*(double)scale;
  x=x-(double)c0*(double)h1;x=x-(double)c1*(double)h2;
  last=(float)x;if(!isfinite(last))return 0;
  double rounded=fp_round_cw(last,g_fpu_cw);
  /* Original converts before saturation. Avoid undefined out-of-int32 casts. */
  if(rounded<INT_MIN||rounded>INT_MAX)return 0;
  int32_t q=(int32_t)rounded;if(q>32767)q=32767;if(q<-32768)q=-32768;
  pcm[i]=(int16_t)q;last_integer=(uint32_t)q;h2=h1;h1=last;
 }
 MEM32(s-24)=g_esi;MEM32(s-20)=g_ebx;MEM32(s-28)=g_ebp;MEM32(s-32)=g_edi;
 memcpy((void*)ADDR(s-16),&last,4);memcpy((void*)ADDR(s-12),&scale,4);memcpy((void*)ADDR(s-8),&c0,4);memcpy((void*)ADDR(s-4),&c1,4);
 memcpy((void*)ADDR(s+12),&last,4);MEM32(s+16)=last_integer;
 for(uint32_t i=0;i<n;i++)memcpy((void*)ADDR(out+i*stride),&pcm[i],2);
 memcpy((void*)ADDR(t+4),&h1,4);memcpy((void*)ADDR(t+8),&h2,4);
 /* Maximum nested depth is four, balanced at return. Bottom four original
  * slots are shifted out; surviving slots and integer shadows retain bits. */
 for(unsigned i=4;i<8;i++){g_st[i]=0.;g_st_i64[i]=0;}
 memcpy(&g_eax,&last,4);g_ecx=g_eax;g_edx=s+16;g_esp=s+20;
 g_flag_a=s-16;g_flag_b=16;g_flag_cf=0;g_flag_k=FK_ADD;g_cur_func=0x561d50;
 return 1;
}
