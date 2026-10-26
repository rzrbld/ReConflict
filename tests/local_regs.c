/* Differential ABI/memory/FPU test against unchanged lifted game routines. */
#include "recomp_types.h"
#include "nonlocal.h"
#include <stdarg.h>
#include <assert.h>
#include <time.h>
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func,g_fs_base,g_gs_base;
uint32_t g_icall_trace[ICALL_TRACE_SIZE],g_icall_from[ICALL_TRACE_SIZE],g_icall_trace_idx,g_icall_count;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;
ptrdiff_t g_mem_base;
extern void global_desertstorm_00540470(void),global_desertstorm_005405A0(void),local_desertstorm_00540470(void),local_desertstorm_005405A0(void);
#define DECLARE(name) extern void global_fixture_##name(void),local_fixture_##name(void);
DECLARE(direct) DECLARE(indirect) DECLARE(tail) DECLARE(fallthrough) DECLARE(cpuid) DECLARE(native_early) DECLARE(setjmp)
static int native_mode;
int ds_test_native_hook(void){if(!native_mode)return 0;g_eax=0xfeedbeef;g_ecx=0x12345678;g_edx=0x87654321;g_esp+=4;g_cur_func=0xc001;return 1;}
int ds_math_fast_540470(void){if(!native_mode)return 0;ds_test_native_hook();g_esp+=8;return 1;}
int ds_math_fast_5405a0(void){return ds_math_fast_540470();}
void native_consumer(void){g_eax+=MEM32(g_esp+4);g_edx^=g_eax;g_ecx+=4;g_esp+=4;g_cur_func=0xaaaa;}
recomp_func_t recomp_lookup_manual(uint32_t va){return va==0xabcd?native_consumer:NULL;}
recomp_func_t recomp_lookup(uint32_t va){(void)va;return NULL;}
recomp_func_t recomp_lookup_import(uint32_t va){(void)va;return NULL;}
void hle_fatal(const char *fmt,...){va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);abort();}
_Noreturn void fixture_throw(void){g_ebp=0xbad1;g_ebx=0xbad2;g_esi=0xbad3;g_edi=0xbad4;PUSH32(g_esp,7);PUSH32(g_esp,0x1000);PUSH32(g_esp,RECOMP_RETADDR);ds_guest_longjmp();}

typedef struct{uint32_t regs[9],flags[4];double st[8];int64_t shadow[8];int top;uint16_t cw;} State;
static void save(State*s){uint32_t regs[]={g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func},flags[]={g_flag_k,g_flag_a,g_flag_b,g_flag_cf};memcpy(s->regs,regs,sizeof regs);memcpy(s->flags,flags,sizeof flags);memcpy(s->st,g_st,sizeof g_st);memcpy(s->shadow,g_st_i64,sizeof g_st_i64);s->top=g_fp_top;s->cw=g_fpu_cw;}
static void restore(const State*s){g_eax=s->regs[0];g_ecx=s->regs[1];g_edx=s->regs[2];g_esp=s->regs[3];g_ebx=s->regs[4];g_esi=s->regs[5];g_edi=s->regs[6];g_ebp=s->regs[7];g_cur_func=s->regs[8];g_flag_k=s->flags[0];g_flag_a=s->flags[1];g_flag_b=s->flags[2];g_flag_cf=s->flags[3];memcpy(g_st,s->st,sizeof g_st);memcpy(g_st_i64,s->shadow,sizeof g_st_i64);g_fp_top=s->top;g_fpu_cw=s->cw;}
static uint32_t rng=0x12345678;
static uint32_t random32(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static uint32_t finite32(void){uint32_t n=random32();if((n&0x7f800000)==0x7f800000)n^=0x00800000;return n;}
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(void){
    g_mem_base=(ptrdiff_t)calloc(1,0x10000);assert(g_mem_base);
    uint8_t initial[0x10000],expected[0x10000];unsigned fast=0,fallback=0;State start={0},want={0},got={0};
    for(unsigned n=0;n<40000;++n){
        for(unsigned i=0;i<sizeof initial;i+=4){uint32_t r=random32();memcpy(initial+i,&r,4);}memcpy((void*)g_mem_base,initial,sizeof initial);
        uint32_t matrix=0x1000,src=0x2000,dst=0x3000,s=0xf000;
        if(n%8==0)dst=src+(n%5)*4-8;
        if(n%8==1)dst=matrix+(n%17)*4;
        if(n%8==2)src=matrix+(n%13)*4;
        if(n%32==3)src=s-12;
        if(n%32==4)dst=s-8;
        if(n%32==5)matrix=s-48;
        for(int i=0;i<16;++i)MEM32(matrix+i*4)=finite32();
        for(int i=0;i<3;++i)MEM32(src+i*4)=finite32();
        if(n%127==0){
            static const uint32_t edges[]={0,0x80000000,1,0x80000001,0x007fffff,0x00800000,0x7f7fffff,0xff7fffff,0x3f800000,0xbf800000};
            for(int i=0;i<16;++i)MEM32(matrix+i*4)=edges[(n/127+i)%10];
            for(int i=0;i<3;++i)MEM32(src+i*4)=edges[(n/127+i+3)%10];
        }
        static const uint32_t modes[]={0,1,2,3,8,10,0x10002,0xffffffff};MEM32(matrix+64)=modes[n%8];
        if(n%31==0)MEM32(src)=0x7fc01234;
        if(n%37==0)MEM32(matrix+36)=0x7f800000;
        g_eax=random32();g_ecx=matrix;g_edx=random32();g_esp=s;g_ebx=random32();g_esi=random32();g_edi=random32();g_ebp=random32();g_cur_func=0x76543210;
        g_flag_k=random32();g_flag_a=random32();g_flag_b=random32();g_flag_cf=random32();
        for(int i=0;i<8;++i){g_st[i]=(double)(int32_t)random32()/128.;g_st_i64[i]=(int64_t)(((uint64_t)random32()<<32)|random32());}g_fp_top=n%9;g_fpu_cw=0x37f;
        MEM32(s)=RECOMP_RETADDR;MEM32(s+4)=src;MEM32(s+8)=dst;
        save(&start);memcpy(initial,(void*)g_mem_base,sizeof initial);
        int point=(n/8)%2;if(point)global_desertstorm_00540470();else global_desertstorm_005405A0();save(&want);memcpy(expected,(void*)g_mem_base,sizeof expected);
        restore(&start);memcpy((void*)g_mem_base,initial,sizeof initial);if(point)local_desertstorm_00540470();else local_desertstorm_005405A0();int done=1;++fast;
        save(&got);
        if(memcmp(&want,&got,sizeof want)||memcmp(expected,(void*)g_mem_base,sizeof expected)){
            fprintf(stderr,"Mismatch case=%u point=%d mode=%#x fast=%d matrix=%#x src=%#x dst=%#x\n",n,point,MEM32(matrix+64),done,matrix,src,dst);
            for(unsigned i=0;i<sizeof want;++i)if(((unsigned char*)&want)[i]!=((unsigned char*)&got)[i]){fprintf(stderr,"state byte=%u want=%#x got=%#x\n",i,((unsigned char*)&want)[i],((unsigned char*)&got)[i]);break;}
            return 1;
        }
    }
    printf("Local registers: %u real hot-function cases; unused=%u; registers, flags, full memory, FPU/shadows PASS\n",fast,fallback);
    struct Pair{void(*global)(void);void(*local)(void);const char*name;} pairs[]={
#define PAIR(name) {global_fixture_##name,local_fixture_##name,#name}
      PAIR(direct),PAIR(indirect),PAIR(tail),PAIR(fallthrough),PAIR(cpuid),PAIR(native_early),PAIR(setjmp),
      {global_desertstorm_00540470,local_desertstorm_00540470,"real_native_hook"}};
    g_fs_base=0x100;
    for(unsigned j=0;j<sizeof pairs/sizeof*pairs;j++)for(unsigned n=0;n<4;n++){
      memset((void*)g_mem_base,0,0x10000);MEM32(g_fs_base)=UINT32_MAX;MEM32(0x2000)=random32();
      g_eax=random32();g_ecx=0x2000;g_edx=random32();g_ebx=random32();g_ebp=random32();g_esi=random32();g_edi=random32();g_esp=0xf000;MEM32(g_esp)=RECOMP_RETADDR;
      native_mode=!strcmp(pairs[j].name,"real_native_hook")?1:n%2;
      save(&start);memcpy(initial,(void*)g_mem_base,sizeof initial);pairs[j].global();save(&want);memcpy(expected,(void*)g_mem_base,sizeof expected);
      restore(&start);memcpy((void*)g_mem_base,initial,sizeof initial);pairs[j].local();save(&got);
      if(memcmp(&want,&got,sizeof want)||memcmp(expected,(void*)g_mem_base,sizeof expected)||ds_jmp_depth()){fprintf(stderr,"ABI fixture mismatch %s case %u\n",pairs[j].name,n);return 1;}
    }
    puts("Local/global direct, indirect, tail, fallthrough, CPUID, native early return, actual guest setjmp/longjmp PASS");native_mode=0;
    /* Relative host-only timing, not a handheld frame-rate claim. */
    g_ecx=0x1000;g_esp=0xf000;MEM32(0x1040)=2;MEM32(0xf004)=0x2000;MEM32(0xf008)=0x3000;
    for(int i=0;i<16;++i){float v=(i+1)/16.f;memcpy((void*)ADDR(0x1000+i*4),&v,4);}for(int i=0;i<3;++i){float v=i+1;memcpy((void*)ADDR(0x2000+i*4),&v,4);}
    const unsigned iterations=1000000;double t=seconds();for(unsigned i=0;i<iterations;++i){g_ecx=0x1000;g_esp=0xf000;global_desertstorm_00540470();}double original=seconds()-t;
    t=seconds();for(unsigned i=0;i<iterations;++i){g_ecx=0x1000;g_esp=0xf000;local_desertstorm_00540470();}double native=seconds()-t;
    printf("Host-only 1M point transforms: globals=%.6fs locals=%.6fs ratio=%.2fx\n",original,native,original/native);free((void*)g_mem_base);return 0;
}
