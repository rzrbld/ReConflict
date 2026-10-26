#define _POSIX_C_SOURCE 200809L
#include "recomp_types.h"
#include "jinvmjt_fast.h"
#include <assert.h>
#include <limits.h>
#include <fenv.h>
#include <time.h>
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;ptrdiff_t g_mem_base;
extern void reference_block_0(void),reference_block_1(void);
enum { MEMORY=0x20000, STACK=0x18000 };
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
 g_mem_base=(ptrdiff_t)malloc(MEMORY);assert(g_mem_base);uint8_t*initial=malloc(MEMORY),*expected=malloc(MEMORY);State start,want,got;unsigned fastn=0,fall=0;
 static const int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
 static const uint32_t edges[]={0,0x80000000,1,0x80000001,0x007fffff,0x00800000,0x7f7fffff,0xff7fffff,0x3f800000,0xbf800000};
 for(unsigned z=0;z<12000;z++){
  assert(!fesetround(modes[(z/2)%4]));for(unsigned i=0;i<MEMORY;i+=4)MEM32(i)=random32();
  uint32_t a=0x2000,b=0x3000,d=0x4000;int accum=z%2;
  switch((z/2)%8){case 1:a=b;break;case 2:b=a+4;break;case 3:d=a;break;case 4:d=b+4;break;case 5:d=a-32;break;case 6:a++;b+=3;d++;break;}
  for(unsigned i=0;i<24;i++){uint32_t v=finite32(),w=finite32();if(z%5==0){v=edges[(z/5+i)%10];w=edges[(z/5+i+3)%10];}memcpy((void*)ADDR(a+4*i),&v,4);memcpy((void*)ADDR(b+4*i),&w,4);}
  for(unsigned i=0;i<16;i++){uint32_t v=finite32();memcpy((void*)ADDR(d+4*i),&v,4);}
  if(z%17==0)MEM32(a+8)=0x7fc01234;if(z%19==0)MEM32(b+12)=0x7f800000;if(z%29==0)MEM32(d+4)=0x7fc0abcd;
  g_eax=z%23==0?3:4;g_ecx=random32();g_edx=random32();g_esp=STACK;g_ebx=b;g_esi=a;g_edi=d;g_ebp=random32();g_cur_func=random32();
  g_flag_k=FK_ADD;g_flag_a=random32();g_flag_b=random32();g_flag_cf=z&1;
  for(unsigned i=0;i<8;i++){uint64_t v=((uint64_t)random32()<<32)|random32();memcpy(&g_st[i],&v,8);g_st_i64[i]=(int64_t)random32();}g_fp_top=(int)(z%11)-2;g_fpu_cw=0x37f|((z%4)<<10);
  save(&start);memcpy(initial,(void*)g_mem_base,MEMORY);
  if(argc>1){g_eax=4;g_esi=0x2000;g_ebx=0x3000;g_edi=0x4000;memset((void*)ADDR(0x2000),0,96);memset((void*)ADDR(0x3000),0,96);memset((void*)ADDR(0x4000),0,64);save(&start);memcpy(initial,(void*)g_mem_base,MEMORY);assert(!ds_jinvmjt_fast_kernel(accum));save(&got);same(&start,&got,initial,z,0);puts("Disabled kernel no mutation PASS");return 0;}
  if(accum)reference_block_1();else reference_block_0();save(&want);memcpy(expected,(void*)g_mem_base,MEMORY);restore(&start);memcpy((void*)g_mem_base,initial,MEMORY);
  int fast=ds_jinvmjt_fast_kernel(accum);if(!fast){save(&got);same(&start,&got,initial,z,0);fall++;if(accum)reference_block_1();else reference_block_0();}else fastn++;
  save(&got);same(&want,&got,expected,z,fast);
 }
 for(unsigned z=0;z<7;z++){
  g_eax=4;g_esi=0x2000;g_ebx=0x3000;g_edi=0x4000;g_fp_top=0;
  if(z==0)g_eax=3;if(z==1)g_esi=UINT32_MAX-90;if(z==2)g_ebx=UINT32_MAX-90;if(z==3)g_edi=UINT32_MAX-60;if(z==4)g_fp_top=INT_MAX-6;if(z==5)g_esi=0;if(z==6)g_edi=0;
  save(&start);memcpy(initial,(void*)g_mem_base,MEMORY);assert(!ds_jinvmjt_fast_kernel(0));save(&got);same(&start,&got,initial,z,0);
 }
 printf("JinvMJT kernels full state PASS: %u accelerated, %u fallback\n",fastn,fall);assert(!fesetround(FE_TONEAREST));g_fp_top=0;
 for(unsigned i=0;i<24;i++){float v=(float)(i%5+1)/32.f;memcpy((void*)ADDR(0x2000+4*i),&v,4);memcpy((void*)ADDR(0x3000+4*i),&v,4);}
 const unsigned loops=200000;double elapsed[2];for(unsigned m=0;m<2;m++){
  for(unsigned k=0;k<2;k++){memset((void*)ADDR(0x4000),0,64);double begin=seconds();for(unsigned i=0;i<loops;i++){g_eax=4;g_esi=0x2000;g_ebx=0x3000;g_edi=0x4000;if(k)assert(ds_jinvmjt_fast_kernel(m));else if(m)reference_block_1();else reference_block_0();}elapsed[k]=seconds()-begin;}
  printf("JinvMJT kernel mode%u %ucalls lifted=%.6fs native=%.6fs ratio=%.3fx (not FPS)\n",m,loops,elapsed[0],elapsed[1],elapsed[0]/elapsed[1]);}
 return 0;
}
