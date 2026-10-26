/* Differential ABI/memory/FPU test against unchanged lifted game routines. */
#include "recomp_types.h"

#include <assert.h>
#include <time.h>
#include <fenv.h>
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;
ptrdiff_t g_mem_base;
extern void reference_004BB500(void),reference_005409A0(void);extern int ds_geometry_fast_004bb500(void),ds_geometry_fast_005409a0(void);
typedef struct{uint32_t regs[9],flags[4];double st[8];int64_t shadow[8];int top;uint16_t cw;} State;
static void save(State*s){uint32_t regs[]={g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func},flags[]={g_flag_k,g_flag_a,g_flag_b,g_flag_cf};memcpy(s->regs,regs,sizeof regs);memcpy(s->flags,flags,sizeof flags);memcpy(s->st,g_st,sizeof g_st);memcpy(s->shadow,g_st_i64,sizeof g_st_i64);s->top=g_fp_top;s->cw=g_fpu_cw;}
static void restore(const State*s){g_eax=s->regs[0];g_ecx=s->regs[1];g_edx=s->regs[2];g_esp=s->regs[3];g_ebx=s->regs[4];g_esi=s->regs[5];g_edi=s->regs[6];g_ebp=s->regs[7];g_cur_func=s->regs[8];g_flag_k=s->flags[0];g_flag_a=s->flags[1];g_flag_b=s->flags[2];g_flag_cf=s->flags[3];memcpy(g_st,s->st,sizeof g_st);memcpy(g_st_i64,s->shadow,sizeof g_st_i64);g_fp_top=s->top;g_fpu_cw=s->cw;}
static uint32_t rng=0x12345678;
static uint32_t random32(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static uint32_t finite32(void){uint32_t n=random32();if((n&0x7f800000)==0x7f800000)n^=0x00800000;return n;}
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(int argc,char**argv){
 if(argc>1&&!strcmp(argv[1],"--disabled")){
  unsetenv("DS_FAST_MATH");State before={0},after={0};g_ecx=UINT32_MAX;g_esp=UINT32_MAX;
  save(&before);assert(!ds_geometry_fast_004bb500());assert(!ds_geometry_fast_005409a0());save(&after);assert(!memcmp(&before,&after,sizeof before));puts("PASS disabled helpers return untouched before guest-pointer access");return 0;
 }
 assert(!setenv("DS_FAST_MATH","1",1));g_mem_base=(ptrdiff_t)calloc(1,0x10000);assert(g_mem_base);
 uint8_t initial[0x10000],expected[0x10000];State start={0},want={0},got={0};unsigned accepted[2][32]={{0}};
 static const uint32_t edge[]={0,0x80000000,1,0x80000001,0x7fffff,0x800000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc01234,0x7f801234};
 static const uint32_t modes[]={0,1,2,3,8,10,0x10002,0xffffffff,4,5,6,7,9,11,12,13,14,15,16,0x10001,0x10008,0x80000002,0x40000000,0x80000000,0x12345678,0x76543210,0xaaaaaaaa,0x55555555,0x7fffffff,0x80000003,0xfffffffd,0xfffffff5};
 if(argc==1)for(unsigned n=0;n<20000;++n){
  static const int rounding[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};assert(!fesetround(rounding[(n/32)%4]));
  for(unsigned i=0;i<sizeof initial;i+=4){uint32_t r=random32();memcpy(initial+i,&r,4);}memcpy((void*)g_mem_base,initial,sizeof initial);
  uint32_t matrix=0x1000,a=0x2000,b=0x3000,s=0xf000;int plane=n%2;
  switch((n/2)%16){
   case 0:b=a;break;case 1:b=a+4;break;case 2:b=a-4;break;
   case 3:a=matrix;break;case 4:b=matrix+8;break;case 5:a=b=matrix;break;
   case 6:a=s-12;break;case 7:b=s-12;break;case 8:matrix=s-80;break;
   case 9:a=s-28;break;case 10:b=s-28;break;default:break;
  }
  for(int i=0;i<17;++i)MEM32(matrix+4*i)=n%3?finite32():edge[(n/3+i)%12];
  for(int i=0;i<3;++i){MEM32(a+4*i)=n%3?finite32():edge[(n/7+i)%12];MEM32(b+4*i)=n%3?finite32():edge[(n/11+i)%12];}
  if(!plane)MEM32(matrix+64)=modes[(n/32)%32];
  g_eax=random32();g_ecx=matrix;g_edx=random32();g_esp=s;g_ebx=random32();g_esi=random32();g_edi=random32();g_ebp=random32();g_cur_func=random32();
  g_flag_k=random32();g_flag_a=random32();g_flag_b=random32();g_flag_cf=random32();
  for(int i=0;i<8;++i){uint64_t bits=((uint64_t)random32()<<32)|random32();memcpy(&g_st[i],&bits,8);g_st_i64[i]=(int64_t)(((uint64_t)random32()<<32)|random32());}
  g_fp_top=(int)(n%9)-4;g_fpu_cw=(uint16_t)random32();
  MEM32(s)=RECOMP_RETADDR;MEM32(s+4)=a;MEM32(s+8)=b;
  save(&start);memcpy(initial,(void*)g_mem_base,sizeof initial);
  if(plane)reference_004BB500();else reference_005409A0();save(&want);memcpy(expected,(void*)g_mem_base,sizeof expected);
  restore(&start);memcpy((void*)g_mem_base,initial,sizeof initial);
  int done=plane?ds_geometry_fast_004bb500():ds_geometry_fast_005409a0();if(done)++accepted[plane][(n/32)%32];if(!done){save(&got);assert(!memcmp(&start,&got,sizeof start));assert(!memcmp(initial,(void*)g_mem_base,sizeof initial));if(plane)reference_004BB500();else reference_005409A0();}save(&got);
  if(memcmp(&want,&got,sizeof want)||memcmp(expected,(void*)g_mem_base,sizeof expected)){
   fprintf(stderr,"Mismatch case=%u plane=%d matrix=%x a=%x b=%x\n",n,plane,matrix,a,b);
   for(unsigned i=0;i<sizeof want;++i)if(((unsigned char*)&want)[i]!=((unsigned char*)&got)[i]){fprintf(stderr,"state byte=%u want=%x got=%x\n",i,((unsigned char*)&want)[i],((unsigned char*)&got)[i]);break;}
   for(unsigned i=0;i<sizeof expected;++i)if(expected[i]!=((unsigned char*)g_mem_base)[i]){fprintf(stderr,"memory byte=%x want=%x got=%x\n",i,expected[i],((unsigned char*)g_mem_base)[i]);break;}return 1;
  }
 }
 if(argc==1){for(unsigned p=0;p<2;++p){printf("accepted helper=%u modes:",p);for(unsigned m=0;m<32;++m)printf(" %u",accepted[p][m]);puts("");}}
 if(argc==1)assert(accepted[0][6]&&accepted[0][7]&&accepted[0][19]&&accepted[0][21]);puts("PASS 20000 exact cases: full 64KiB guest memory, all GPRs/current function, flags, raw FP/shadows/depth/CW; argument/matrix/scratch aliases and nonfinites.");
 assert(!fesetround(FE_TONEAREST));
 for(unsigned plane=0;plane<2;++plane)for(unsigned mode=0;mode<12;++mode){
  static const unsigned flags[]={1,2,8,0,0x10002,0xffffffff,0x10001,0x10008,5,7,13,15};uint32_t s=0xf000;
  memset((void*)g_mem_base,0,0x10000);for(int i=0;i<16;++i){float v=(i+1)/32.f;memcpy((void*)ADDR(0x1000+4*i),&v,4);}MEM32(0x1040)=flags[mode];MEM32(s+4)=0x2000;MEM32(s+8)=0x3000;
  const unsigned iterations=200000;double t=seconds();for(unsigned i=0;i<iterations;++i){g_ecx=0x1000;g_esp=s;if(plane)reference_004BB500();else reference_005409A0();}double orig=seconds()-t;
  t=seconds();for(unsigned i=0;i<iterations;++i){g_ecx=0x1000;g_esp=s;if(plane)ds_geometry_fast_004bb500();else ds_geometry_fast_005409a0();}double fast=seconds()-t;
  printf("helper=%s flags=%u iterations=%u original-reference=%.6f staged=%.6f ratio=%.3f\n",plane?"4BB500":"5409A0",flags[mode],iterations,orig,fast,orig/fast);
  if(plane)break;
 }
 free((void*)g_mem_base);return 0;
}
