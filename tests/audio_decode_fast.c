#define _POSIX_C_SOURCE 200809L
#include "recomp_types.h"
#include "audio_decode_fast.h"
#include <assert.h>
#include <limits.h>
#include <fenv.h>
#include <time.h>
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;ptrdiff_t g_mem_base;
extern void reference_00561D50(void);
enum { MEMORY=0x600000, STACK=0x18000 };
typedef struct{uint32_t regs[9],flags[4];double st[8];int64_t shadow[8];int top;uint16_t cw;} State;
static void save(State*s){
    memset(s,0,sizeof *s);
    uint32_t regs[]={g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func};
    uint32_t flags[]={g_flag_k,g_flag_a,g_flag_b,g_flag_cf};
    memcpy(s->regs,regs,sizeof regs);memcpy(s->flags,flags,sizeof flags);
    memcpy(s->st,g_st,sizeof g_st);memcpy(s->shadow,g_st_i64,sizeof g_st_i64);s->top=g_fp_top;s->cw=g_fpu_cw;
}
static void restore(const State*s){
    g_eax=s->regs[0];g_ecx=s->regs[1];g_edx=s->regs[2];g_esp=s->regs[3];
    g_ebx=s->regs[4];g_esi=s->regs[5];g_edi=s->regs[6];g_ebp=s->regs[7];g_cur_func=s->regs[8];
    g_flag_k=s->flags[0];g_flag_a=s->flags[1];g_flag_b=s->flags[2];g_flag_cf=s->flags[3];
    memcpy(g_st,s->st,sizeof g_st);memcpy(g_st_i64,s->shadow,sizeof g_st_i64);g_fp_top=s->top;g_fpu_cw=s->cw;
}
static uint32_t rng=0x529520;
static uint32_t random32(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static uint32_t finite32(void){uint32_t r=random32();if((r&0x7f800000)==0x7f800000)r^=0x00800000;return r;}
static void same(const State*a,const State*b,const uint8_t*want,unsigned n,int fast){
    if(!memcmp(a,b,sizeof *a)&&!memcmp(want,(void*)g_mem_base,MEMORY))return;
    fprintf(stderr,"Mismatch case=%u fast=%d\n",n,fast);
    for(unsigned i=0;i<sizeof *a;i++)if(((const uint8_t*)a)[i]!=((const uint8_t*)b)[i]){
        fprintf(stderr,"state byte%u want%02x got%02x\n",i,((const uint8_t*)a)[i],((const uint8_t*)b)[i]);break;
    }
    for(unsigned i=0;i<MEMORY;i++)if(want[i]!=((const uint8_t*)g_mem_base)[i]){
        fprintf(stderr,"memory%#x want%02x got%02x\n",i,want[i],((const uint8_t*)g_mem_base)[i]);break;
    }
    for(unsigned i=0;i<9;i++)fprintf(stderr,"reg%u want%#x got%#x\n",i,a->regs[i],b->regs[i]);
    for(unsigned i=0;i<4;i++)fprintf(stderr,"flag%u want%#x got%#x\n",i,a->flags[i],b->flags[i]);
    fprintf(stderr,"top want%d got%d\n",a->top,b->top);exit(1);
}
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(int argc,char**argv){
 g_mem_base=(ptrdiff_t)calloc(1,MEMORY);assert(g_mem_base);uint8_t*initial=malloc(MEMORY),*expected=malloc(MEMORY);State start,want,got;unsigned accelerated=0,fallback=0;
 float coeff[]={0,0,-.9375f,0,-1.796875f,.8125f,-1.53125f,.859375f,-1.90625f,.9375f};memcpy((void*)ADDR(0x5fe64c),coeff,sizeof coeff);MEM32(0x5d843c)=0x3f800000;
 for(unsigned z=0;z<2400;z++){
  static const int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};assert(!fesetround(modes[(z/4)%4]));
  uint32_t t=0x1000,in=0x2000,out=0x6000,n=(z==2399?4096:z%257),s=STACK;
  for(unsigned i=0;i<n;i++)MEM16(in+2*i)=(uint16_t)random32();
  if(z%13==0)out=in;if(z%17==0)out=t;if(z%19==0)out=s-32;
  float h1=(int16_t)random32(),h2=(int16_t)random32();memcpy((void*)ADDR(t+4),&h1,4);memcpy((void*)ADDR(t+8),&h2,4);MEM32(t+20)=z%5;MEM32(t+24)=(z/5)%16;
  if(z%23==0)MEM32(t+4)=0x7fc01234;
  else if(z%41==0){MEM32(t+4)=0x80000000;MEM32(t+8)=1;}
  g_eax=random32();g_ecx=t;g_edx=random32();g_esp=s;g_ebx=random32();g_esi=random32();g_edi=random32();g_ebp=random32();g_cur_func=0;
  g_flag_k=FK_ADD;g_flag_a=random32();g_flag_b=random32();g_flag_cf=z&1;
  for(unsigned i=0;i<8;i++){uint64_t raw=((uint64_t)random32()<<32)|random32();memcpy(&g_st[i],&raw,8);g_st_i64[i]=(int64_t)random32();}g_fp_top=(int)(z%11)-2;g_fpu_cw=0x37f|((z%4)<<10);
  MEM32(s)=RECOMP_RETADDR;MEM32(s+4)=in;MEM32(s+8)=out;MEM32(s+12)=n;MEM32(s+16)=z%3;
  save(&start);memcpy(initial,(void*)g_mem_base,MEMORY);
  if(argc>1){assert(!ds_audio_decode_fast_561d50());save(&got);same(&start,&got,initial,z,0);puts("Disabled path has no mutation PASS");return 0;}
  reference_00561D50();save(&want);memcpy(expected,(void*)g_mem_base,MEMORY);restore(&start);memcpy((void*)g_mem_base,initial,MEMORY);
  int fast=ds_audio_decode_fast_561d50();if(!fast){save(&got);same(&start,&got,initial,z,0);fallback++;reference_00561D50();}else accelerated++;
  save(&got);same(&want,&got,expected,z,fast);
 }
 for(unsigned z=0;z<7;z++){
  g_esp=STACK;g_ecx=0x1000;g_fp_top=0;MEM32(STACK+4)=0x2000;MEM32(STACK+8)=0x6000;MEM32(STACK+12)=28;MEM32(STACK+16)=0;MEM32(0x1014)=2;MEM32(0x1018)=8;
  if(z==0)g_esp=31;if(z==1)g_ecx=UINT32_MAX-10;if(z==2)MEM32(STACK+12)=4097;if(z==3)MEM32(0x1014)=5;if(z==4)MEM32(0x1018)=16;if(z==5)g_fp_top=INT_MAX-3;if(z==6)MEM32(0x1004)=0x7f7fffff;
  save(&start);memcpy(initial,(void*)g_mem_base,MEMORY);assert(!ds_audio_decode_fast_561d50());save(&got);same(&start,&got,initial,z,0);
 }
 printf("ADPCM full memory/GPR/flags/x87/shadow/CW PASS: %u accelerated, %u fallback\n",accelerated,fallback);
 assert(!fesetround(FE_TONEAREST));
 MEM32(0x1004)=0;MEM32(0x1008)=0;MEM32(0x1014)=2;MEM32(0x1018)=8;
 const unsigned loops=100000;double times[2];for(unsigned k=0;k<2;k++){double begin=seconds();for(unsigned i=0;i<loops;i++){g_esp=STACK;g_ecx=0x1000;MEM32(STACK+4)=0x2000;MEM32(STACK+8)=0x6000;MEM32(STACK+12)=28;MEM32(STACK+16)=1;if(k)assert(ds_audio_decode_fast_561d50());else reference_00561D50();}times[k]=seconds()-begin;}
 printf("ADPCM kernel %u x28samples lifted=%.6fs native=%.6fs speedup=%.3fx (not gameplay FPS)\n",loops,times[0],times[1],times[0]/times[1]);return 0;
}
