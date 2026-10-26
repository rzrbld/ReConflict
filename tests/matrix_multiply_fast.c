#define _POSIX_C_SOURCE 200809L
#include "recomp_types.h"
#include "matrix_multiply_fast.h"
#include <assert.h>
#include <limits.h>
#include <fenv.h>
#include <time.h>
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;ptrdiff_t g_mem_base;
extern void reference_004AC1FB(void);
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
    g_mem_base=(ptrdiff_t)malloc(MEMORY);assert(g_mem_base);
    uint8_t *initial=malloc(MEMORY),*expected=malloc(MEMORY);assert(initial&&expected);
    State start,want,got;unsigned fast_count=0,fallback_count=0,by_case[12]={0};
    static const uint32_t edges[]={0,0x80000000,1,0x80000001,0x007fffff,0x00800000,0x7f7fffff,0xff7fffff,0x3f800000,0xbf800000};
    static const uint16_t controls[]={0x37f,0x27f,0x7f,0x77f,0xb7f,0xf7f};
    static const int rounding[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    for(unsigned n=0;n<24000;n++){
        assert(!fesetround(rounding[(n/12)%4]));
        for(unsigned i=0;i<MEMORY;i+=4){uint32_t r=random32();memcpy((void*)ADDR(i),&r,4);}
        uint32_t left=0x2000,right=0x3000,dst=0x4000,s=STACK;
        switch(n%12){
          case 1:dst=left;break;case 2:dst=right;break;case 3:right=dst=left;break;
          case 4:right=left+4;break;case 5:right=left;break;
          case 6:dst=left+4;break;case 7:dst=right-4;break;
          case 8:left=s-68;break;case 9:right=s-64;break;
          case 10:dst=s-60;break;case 11:left=s+4;break;
        }
        for(unsigned i=0;i<16;i++){MEM32(left+4*i)=finite32();MEM32(right+4*i)=finite32();}
        if(n%17==0)for(unsigned i=0;i<16;i++){MEM32(left+i*4)=edges[(n/17+i)%10];MEM32(right+i*4)=edges[(n/17+i+3)%10];}
        if(n%31==0&&n%12<8)MEM32(left+8)=0x7fc01234;
        if(n%37==0&&n%12<8)MEM32(right+12)=0xff800000;
        g_eax=random32();g_ecx=random32();g_edx=random32();g_esp=s;g_ebx=random32();g_esi=random32();g_edi=random32();g_ebp=random32();g_cur_func=random32();
        g_flag_k=random32();g_flag_a=random32();g_flag_b=random32();g_flag_cf=random32();
        for(int i=0;i<8;i++){g_st[i]=(double)(int32_t)random32()/256.;g_st_i64[i]=(int64_t)(((uint64_t)random32()<<32)|random32());}
        g_fp_top=(int)(n%11)-2;g_fpu_cw=controls[(n/48)%6];
        MEM32(s)=RECOMP_RETADDR;MEM32(s+4)=dst;MEM32(s+8)=left;MEM32(s+12)=right;
        save(&start);memcpy(initial,(void*)g_mem_base,MEMORY);
        if(argc>1&&!strcmp(argv[1],"--disabled")){
            assert(!ds_matrix_multiply_fast_4ac1fb());save(&got);same(&start,&got,initial,n,0);
            printf("DS_FAST_MATH=0: no state or memory mutation PASS\n");goto done;
        }
        reference_004AC1FB();save(&want);memcpy(expected,(void*)g_mem_base,MEMORY);
        restore(&start);memcpy((void*)g_mem_base,initial,MEMORY);
        int fast=ds_matrix_multiply_fast_4ac1fb();
        if(!fast){save(&got);same(&start,&got,initial,n,0);++fallback_count;reference_004AC1FB();}
        else {++fast_count;++by_case[n%12];}
        save(&got);same(&want,&got,expected,n,fast);
    }
    /* Range/depth declines must inspect no invalid matrix bytes or mutate ABI. */
    for(unsigned n=0;n<5;n++){
        g_esp=STACK;MEM32(STACK+4)=0x4000;MEM32(STACK+8)=0x2000;MEM32(STACK+12)=0x3000;g_fp_top=0;
        if(n==0)g_esp=83;
        if(n==1)g_esp=UINT32_MAX-12;
        if(n==2)MEM32(STACK+4)=UINT32_MAX-32;
        if(n==3)MEM32(STACK+8)=UINT32_MAX-32;
        if(n==4)g_fp_top=INT_MAX-15;
        save(&start);memcpy(initial,(void*)g_mem_base,MEMORY);assert(!ds_matrix_multiply_fast_4ac1fb());save(&got);same(&start,&got,initial,n,0);
    }
    for(unsigned i=0;i<6;i++)assert(by_case[i]>1000);
    assert(!fesetround(FE_TONEAREST));
    printf("Matrix multiply: %u accelerated, %u unchanged fallbacks; registers/flags/full-memory/stack/FPU/shadows, aliases, finite edges, nonfinite declines, four host rounding modes and six CW values PASS\n",fast_count,fallback_count);
    for(unsigned i=0;i<16;i++){float x=(float)(i+1)/16.f,y=(float)(17-i)/8.f;memcpy((void*)ADDR(0x2000+i*4),&x,4);memcpy((void*)ADDR(0x3000+i*4),&y,4);}
    MEM32(STACK+4)=0x4000;MEM32(STACK+8)=0x2000;MEM32(STACK+12)=0x3000;g_fp_top=0;g_fpu_cw=0x37f;
    const unsigned iterations=300000;double t=seconds();
    for(unsigned i=0;i<iterations;i++){g_esp=STACK;reference_004AC1FB();}double original=seconds()-t;
    t=seconds();for(unsigned i=0;i<iterations;i++){g_esp=STACK;assert(ds_matrix_multiply_fast_4ac1fb());}double native=seconds()-t;
    printf("Kernel-only %u matrix multiplies: lifted=%.6fs native=%.6fs ratio=%.2fx (not gameplay FPS)\n",iterations,original,native,original/native);
done:
    free(initial);free(expected);free((void*)g_mem_base);return 0;
}
