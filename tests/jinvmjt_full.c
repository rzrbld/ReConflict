#define _POSIX_C_SOURCE 200809L
#include "recomp_types.h"
#include "jinvmjt_fast.h"
#include <assert.h>
#include <limits.h>
#include <fenv.h>
#include <time.h>
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;ptrdiff_t g_mem_base;
extern void reference_00595270(void),native_00595270(void);
unsigned ds_accepted[2],ds_declined;
uint32_t g_icall_trace[ICALL_TRACE_SIZE],g_icall_from[ICALL_TRACE_SIZE],g_icall_trace_idx,g_icall_count;
recomp_func_t recomp_lookup(uint32_t a){fprintf(stderr,"Unexpected profiling dispatch %#x\n",a);abort();}
recomp_func_t recomp_lookup_manual(uint32_t a){return recomp_lookup(a);}recomp_func_t recomp_lookup_import(uint32_t a){return recomp_lookup(a);}
enum { MEMORY=0x800000, STACK=0x700000 };
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
int main(void){
 g_mem_base=(ptrdiff_t)calloc(1,MEMORY);assert(g_mem_base);uint8_t*initial=malloc(MEMORY),*expected=malloc(MEMORY);State start,want,got;
 static const int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
 for(unsigned z=0;z<240;z++){
  memset((void*)g_mem_base,0,MEMORY);assert(!fesetround(modes[z%4]));unsigned blocks=z%5;
  uint32_t obj=0x1000,a=0x2000,b=0x10000,counts=0x20000,indices=0x21000,mat=0x30000,out=0x40000,vec=0x50000;
  MEM32(obj+0x14)=blocks?blocks*4-z%4:0;MEM32(obj+0x18)=blocks*4;MEM32(obj+0x1c)=blocks;MEM32(obj+0x24)=mat;MEM32(obj+0x28)=out;MEM32(obj+0x2c)=0x60000;MEM32(obj+0x30)=0x61000;MEM32(obj+0x34)=0x62000;
  float v=.001f;memcpy((void*)ADDR(obj+8),&v,4);v=.1f;memcpy((void*)ADDR(obj+12),&v,4);
  unsigned total=0;for(unsigned k=0;k<blocks;k++){unsigned n=1+(z+k)%3;MEM32(counts+k*4)=n;for(unsigned j=0;j<n;j++)MEM32(indices+k*32+j*4)=z%13==0?UINT32_MAX:j;total+=n;}
  for(unsigned k=0;k<total*24;k++){v=(float)(int16_t)random32()/32768.f;memcpy((void*)ADDR(a+k*4),&v,4);v=(float)(int16_t)random32()/32768.f;memcpy((void*)ADDR(b+k*4),&v,4);}
  for(unsigned k=0;k<blocks*4;k++){v=(float)(int16_t)random32()/65536.f;memcpy((void*)ADDR(vec+k*4),&v,4);}
  if(z%17==0&&total)MEM32(a+8)=0x7fc01234;if(z%19==0&&total)MEM32(b+12)=0x7f800000;
  g_eax=random32();g_ecx=obj;g_edx=random32();g_esp=STACK;g_ebx=random32();g_esi=random32();g_edi=random32();g_ebp=random32();g_cur_func=random32();g_flag_k=FK_ADD;g_flag_a=random32();g_flag_b=random32();g_flag_cf=z&1;
  for(unsigned i=0;i<8;i++){uint64_t bits=((uint64_t)random32()<<32)|random32();memcpy(&g_st[i],&bits,8);g_st_i64[i]=(int64_t)random32();}g_fp_top=(int)(z%11)-2;g_fpu_cw=0x37f|((z%4)<<10);
  MEM32(STACK)=RECOMP_RETADDR;MEM32(STACK+4)=a;MEM32(STACK+8)=b;MEM32(STACK+12)=counts;MEM32(STACK+16)=indices;MEM32(STACK+20)=vec;
  save(&start);memcpy(initial,(void*)g_mem_base,MEMORY);reference_00595270();save(&want);memcpy(expected,(void*)g_mem_base,MEMORY);assert(!g_icall_count);restore(&start);memcpy((void*)g_mem_base,initial,MEMORY);
  native_00595270();save(&got);same(&want,&got,expected,z,1);assert(!g_icall_count);
 }
 printf("JinvMJT mode hits overwrite=%u accumulate=%u declined=%u\n",ds_accepted[0],ds_accepted[1],ds_declined);if(getenv("DS_FAST_MATH")&&!strcmp(getenv("DS_FAST_MATH"),"1"))assert(ds_accepted[0]&&ds_accepted[1]&&ds_declined);else assert(!ds_accepted[0]&&!ds_accepted[1]&&ds_declined);
 puts("JinvMJT original full function240fixtures/complete8MiB/GPR/flags/rawFPU/shadows/top/CW, original profiling-disabled callees PASS");return 0;
}
