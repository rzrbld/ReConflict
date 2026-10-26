#define _POSIX_C_SOURCE 200809L
#include "recomp_types.h"
#include "length_scalar.h"
#include <assert.h>
#include <limits.h>
#include <fenv.h>
#include <time.h>
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;ptrdiff_t g_mem_base;
extern void reference_0051FCC0(void);
enum { MEMORY=0x20000, BASE=0x5C0000, STACK=BASE+0x18000 };
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
    if(!memcmp(a,b,sizeof *a)&&!memcmp(want,(void*)(g_mem_base+BASE),MEMORY))return;
    fprintf(stderr,"Mismatch case=%u fast=%d\n",n,fast);
    for(unsigned i=0;i<sizeof *a;i++)if(((const uint8_t*)a)[i]!=((const uint8_t*)b)[i]){
        fprintf(stderr,"state byte%u want%02x got%02x\n",i,((const uint8_t*)a)[i],((const uint8_t*)b)[i]);break;
    }
    for(unsigned i=0;i<MEMORY;i++)if(want[i]!=((const uint8_t*)(g_mem_base+BASE))[i]){
        fprintf(stderr,"memory%#x want%02x got%02x\n",i,want[i],((const uint8_t*)(g_mem_base+BASE))[i]);break;
    }
    for(unsigned i=0;i<9;i++)fprintf(stderr,"reg%u want%#x got%#x\n",i,a->regs[i],b->regs[i]);
    for(unsigned i=0;i<4;i++)fprintf(stderr,"flag%u want%#x got%#x\n",i,a->flags[i],b->flags[i]);
    fprintf(stderr,"top want%d got%d\n",a->top,b->top);exit(1);
}
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(int argc,char**argv){
 (void)argv;
 uint8_t*memory=malloc(MEMORY),*initial=malloc(MEMORY),*expected=malloc(MEMORY);assert(memory&&initial&&expected);
 g_mem_base=(ptrdiff_t)memory-BASE;State start,want,got;unsigned fastn=0,fallback=0;
 static const int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
 static const uint32_t edges[]={0,0x80000000,1,0x80000001,0x007fffff,0x00800000,0x5effffff,0x5f000000,0x7f7fffff,0xff7fffff,0x3f800000,0xbf800000,0x7fc12345,0x7f812345,0x7f800000,0xff800000};
 for(unsigned z=0;z<16000;z++){
  assert(!fesetround(modes[z%4]));for(unsigned i=0;i<MEMORY;i+=4)MEM32(BASE+i)=random32();
  uint32_t src=BASE+0x2000,s=STACK;
  switch((z/4)%12){case 1:src=s;break;case 2:src=s+4;break;case 3:src=s-4;break;case 4:src++;break;case 5:src=0x5D8364;break;case 6:s=0x5D8764;break;case 7:s=0x5D8438;break;}
  MEM32(0x5D8364)=0x3f000000;MEM32(0x5D8768)=0x3fc00000;MEM32(0x5D843C)=0x3f800000;
  if(z%11==0)MEM32(0x5D8364)=random32();
  if(z%13==0)MEM32(0x5D8768)=random32();
  if(z%17==0)MEM32(0x5D843C)=edges[(z/17)%16];
  for(unsigned i=0;i<3;i++){uint32_t b=finite32();if(z%5==0)b=edges[(z/5+i)%16];memcpy((void*)ADDR(src+4*i),&b,4);}
  // Explicit zero and near-overflow vectors preserve the original approximation.
  if(z%23==0)for(unsigned i=0;i<3;i++)MEM32(src+4*i)=0;
  if(z%29==0)for(unsigned i=0;i<3;i++)MEM32(src+4*i)=0x5effffff;
  g_eax=random32();g_ecx=random32();g_edx=random32();g_esp=s;g_ebx=random32();g_esi=random32();g_edi=random32();g_ebp=random32();g_cur_func=0;
  g_flag_k=FK_ADD;g_flag_a=random32();g_flag_b=random32();g_flag_cf=z&1;
  for(unsigned i=0;i<8;i++){uint64_t b=((uint64_t)random32()<<32)|random32();memcpy(&g_st[i],&b,8);g_st_i64[i]=(int64_t)(((uint64_t)random32()<<32)|random32());}
  g_fp_top=(int)(z%11)-2;g_fpu_cw=0x37f|((z%4)<<10);
  MEM32(s)=RECOMP_RETADDR;MEM32(s+4)=src;save(&start);memcpy(initial,memory,MEMORY);
  if(argc>1){assert(!ds_length_0051FCC0());save(&got);same(&start,&got,initial,z,0);puts("Disabled length path PASS");return 0;}
  reference_0051FCC0();save(&want);memcpy(expected,memory,MEMORY);restore(&start);memcpy(memory,initial,MEMORY);
  int fast=ds_length_0051FCC0();if(fast)fastn++;else{save(&got);same(&start,&got,initial,z,0);fallback++;reference_0051FCC0();}
  save(&got);same(&want,&got,expected,z,fast);
 }
 for(unsigned k=0;k<4;k++){
  g_esp=STACK;MEM32(STACK+4)=BASE+0x2000;g_fp_top=0;
  if(k==0)g_fp_top=INT_MAX-4;if(k==1)g_esp=UINT32_MAX-4;if(k==2)MEM32(STACK+4)=0;if(k==3)MEM32(STACK+4)=UINT32_MAX-8;
  save(&start);memcpy(initial,memory,MEMORY);assert(!ds_length_0051FCC0());save(&got);same(&start,&got,initial,k,0);
 }
 assert(fastn>1000&&fallback>1000);
 printf("Length full-state/full-memory PASS: %u accelerated, %u fallback; four rounding modes, boundary/NaN/infinity/alias cases\n",fastn,fallback);
 assert(!fesetround(FE_TONEAREST));MEM32(0x5D8364)=0x3f000000;MEM32(0x5D8768)=0x3fc00000;MEM32(0x5D843C)=0x3f800000;
 for(unsigned i=0;i<3;i++){float x=(float)(i+1)*19.5f;memcpy((void*)ADDR(BASE+0x2000+4*i),&x,4);}
 const unsigned loops=500000;double elapsed[2];
 for(unsigned k=0;k<2;k++){
  double begin=seconds();for(unsigned i=0;i<loops;i++){
   g_esp=STACK;g_fp_top=0;MEM32(STACK+4)=BASE+0x2000;if(k)assert(ds_length_0051FCC0());else reference_0051FCC0();
  }elapsed[k]=seconds()-begin;
 }
 printf("Length kernel %u calls: lifted=%.6fs scalar=%.6fs ratio=%.3fx (not gameplay FPS)\n",loops,elapsed[0],elapsed[1],elapsed[0]/elapsed[1]);
 free(memory);free(initial);free(expected);return 0;
}
