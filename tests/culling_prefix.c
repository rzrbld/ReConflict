/* Full guest-state differential tests against the unchanged translated
 * spatial traversal, including declined callbacks and eight-child traversal. */
#include "recomp_types.h"
#include "culling_fast.h"
#include <assert.h>
#include <time.h>
#include <fenv.h>
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func;
uint32_t g_icall_trace[ICALL_TRACE_SIZE],g_icall_from[ICALL_TRACE_SIZE],g_icall_trace_idx,g_icall_count;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;
ptrdiff_t g_mem_base;
extern void reference_0050C350(void);extern void candidate_0050C350(void);
static unsigned callbacks;
static void callback(void){assert(MEM32(g_esp+8)==0x7654);MEM32(0x9000)+=MEM32(g_esp+4);MEM32(0x9004)=MEM32(0x9004)*33u+MEM32(g_esp+4);if(MEM32(g_esp+4)==0x4000)MEM32(0x4024)=0xc3480000u;++callbacks;g_eax=0xabcdef12;g_ecx=0x12345678;g_edx=0x87654321;g_esp+=4;}
recomp_func_t recomp_lookup(uint32_t a){if(a==0x123456)return callback;fprintf(stderr,"Unexpected call %#x\n",a);abort();}
recomp_func_t recomp_lookup_manual(uint32_t a){(void)a;return NULL;}
recomp_func_t recomp_lookup_import(uint32_t a){(void)a;return NULL;}
void recomp_dump_trace(const char*s){fprintf(stderr,"%s\n",s);}
typedef struct{uint32_t regs[9],flags[4],calls;double fp[8];int64_t shadow[8];int top;uint16_t cw;uint32_t trace[ICALL_TRACE_SIZE],from[ICALL_TRACE_SIZE],index,count;} State;
static void save(State*s){uint32_t r[]={g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func},f[]={g_flag_k,g_flag_a,g_flag_b,g_flag_cf};memcpy(s->regs,r,sizeof r);memcpy(s->flags,f,sizeof f);s->calls=callbacks;memcpy(s->fp,g_st,sizeof g_st);memcpy(s->shadow,g_st_i64,sizeof g_st_i64);s->top=g_fp_top;s->cw=g_fpu_cw;memcpy(s->trace,g_icall_trace,sizeof g_icall_trace);memcpy(s->from,g_icall_from,sizeof g_icall_from);s->index=g_icall_trace_idx;s->count=g_icall_count;}
static void restore(const State*s){g_eax=s->regs[0];g_ecx=s->regs[1];g_edx=s->regs[2];g_esp=s->regs[3];g_ebx=s->regs[4];g_esi=s->regs[5];g_edi=s->regs[6];g_ebp=s->regs[7];g_cur_func=s->regs[8];g_flag_k=s->flags[0];g_flag_a=s->flags[1];g_flag_b=s->flags[2];g_flag_cf=s->flags[3];callbacks=s->calls;memcpy(g_st,s->fp,sizeof g_st);memcpy(g_st_i64,s->shadow,sizeof g_st_i64);g_fp_top=s->top;g_fpu_cw=s->cw;memcpy(g_icall_trace,s->trace,sizeof g_icall_trace);memcpy(g_icall_from,s->from,sizeof g_icall_from);g_icall_trace_idx=s->index;g_icall_count=s->count;}
static uint32_t rng=0xa5df1293;
static uint32_t random32(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static uint32_t finite32(void){uint32_t n=random32();if((n&0x7f800000)==0x7f800000)n^=0x00800000;return n;}
static void put(uint32_t p,float f){memcpy((void*)ADDR(p),&f,4);}
static void registers(unsigned n){g_eax=random32();g_ecx=0x1000;g_edx=random32();g_esp=0xf000;g_ebx=random32();g_esi=random32();g_edi=random32();g_ebp=random32();g_cur_func=random32();g_flag_k=random32();g_flag_a=random32();g_flag_b=random32();g_flag_cf=random32();for(int i=0;i<8;++i){g_st[i]=(int32_t)random32()/137.;g_st_i64[i]=(int64_t)(((uint64_t)random32()<<32)|random32());}if(n%17==0){static const uint64_t payloads[]={0x8000000000000000ull,0x7ff8000000001234ull,0x7ff0000000001234ull,1,0xffffffffffffffffull};for(int i=0;i<8;++i)memcpy(&g_st[i],&payloads[i%5],8);}g_fp_top=n%9;g_fpu_cw=0x37f|((n%4)<<10);MEM32(g_esp)=RECOMP_RETADDR;callbacks=0;g_icall_trace_idx=g_icall_count=0;memset(g_icall_trace,0,sizeof g_icall_trace);memset(g_icall_from,0,sizeof g_icall_from);}
static int compare(unsigned n,uint8_t*initial,uint8_t*expected){
    State start={0},want={0},got={0};save(&start);memcpy(initial,(void*)g_mem_base,0x800000);reference_0050C350();save(&want);memcpy(expected,(void*)g_mem_base,0x800000);
    restore(&start);memcpy((void*)g_mem_base,initial,0x800000);candidate_0050C350();int done=1;
    save(&got);if(memcmp(&want,&got,sizeof want)||memcmp(expected,(void*)g_mem_base,0x800000)){
        fprintf(stderr,"Culling mismatch case=%u fast=%d node=%#x\n",n,done,start.regs[1]);
        for(unsigned p=0xefe8;p<0xf000;p+=4){uint32_t a,b;memcpy(&a,expected+p,4);memcpy(&b,(void*)ADDR(p),4);fprintf(stderr,"scratch%#x want=%08x got=%08x\n",p,a,b);}
        fprintf(stderr,"rejected=%u node radius/xyz=%08x %08x %08x %08x\n",g_edx,MEM32(0x1008),MEM32(0x100c),MEM32(0x1010),MEM32(0x1014));
        for(unsigned i=0;i<sizeof want;++i)if(((uint8_t*)&want)[i]!=((uint8_t*)&got)[i]){fprintf(stderr,"state byte%u expected%02x got%02x\n",i,((uint8_t*)&want)[i],((uint8_t*)&got)[i]);break;}
        for(unsigned i=0;i<0x800000;++i)if(expected[i]!=((uint8_t*)g_mem_base)[i]){fprintf(stderr,"memory%#x expected%02x got%02x\n",i,expected[i],((uint8_t*)g_mem_base)[i]);break;}
        exit(1);
    }
    return done;
}
int main(int argc,char**argv){
    if(argc==2&&!strcmp(argv[1],"--disabled")){g_esp=g_esi=UINT32_MAX;State a={0},b={0};save(&a);assert(!ds_culling_prefix_50c359());save(&b);assert(!memcmp(&a,&b,sizeof a));puts("Disabled prefix no mutation/no guest reads PASS");return 0;}
    int benchmark_only=argc==2&&!strcmp(argv[1],"--benchmark-only");
    if(argc>1&&!benchmark_only){fprintf(stderr,"Usage: %s [--benchmark-only]\n",argv[0]);return 2;}

    assert(!setenv("DS_FAST_CULLING","1",1));g_mem_base=(ptrdiff_t)calloc(1,0x800000);uint8_t*initial=malloc(0x800000),*expected=malloc(0x800000);assert(g_mem_base&&initial&&expected);
    unsigned accelerated=0,fallback=0;
    if(!benchmark_only){
    for(unsigned n=0;n<5000;++n){
        /* Randomize every byte that reference execution could write. The
         * entire allocated 8MiB guest image is compared, including globals. */
        for(unsigned i=0;i<0x10000;i+=4)MEM32(i)=random32();
        put(0x5dad3c,1.7320507764816284f);put(0x5d8344,0);MEM32(0x644024)=0x3000;MEM32(0x644028)=0x123456;MEM32(0x64405c)=0x7654;MEM32(0x5fc060)=random32();
        registers(n);MEM32(0x1004)=0;
        static const int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};assert(!fesetround(modes[(n/4)%4]));
        unsigned count=1+n%16;MEM32(0x3000)=count;
        for(unsigned i=0;i<count*4;++i)MEM32(0x3010+i*4)=finite32();
        for(unsigned i=0;i<4;++i)MEM32(0x1008+i*4)=finite32();
        if(n%3==0){
            put(0x1008,1);put(0x100c,0);put(0x1010,0);put(0x1014,0);
            for(unsigned i=0;i<count;++i){put(0x3010+i*16,1);put(0x3014+i*16,0);put(0x3018+i*16,0);put(0x301c+i*16,10);}
            put(0x301c+(n%count)*16,-10); /* reject at every possible plane */
        }
        if(n%23==0){
            static const uint32_t edges[]={0,0x80000000,1,0x80000001,0x007fffff,0x00800000,0x7f7fffff,0xff7fffff,0x3f800000,0xbf800000};
            for(int i=0;i<4;++i)MEM32(0x1008+i*4)=edges[(n/23+i)%10];
        }
        if(n%29==0)MEM32(0x3014)=0x7fc01234; /* nonfinite fallback */
        if(n%31==0)MEM32(0x1008)=0x7f800000;
        if(n%37==0){ /* Decline visible leaves; their real callback must run. */
            put(0x1008,1);put(0x100c,0);put(0x1010,0);put(0x1014,0);MEM32(0x3000)=1;put(0x3010,1);put(0x3014,0);put(0x3018,0);put(0x301c,10);
        }
        if(n%41==0){ /* Tangency: compare AFTER original float rounding. */
            MEM32(0x3000)=1;put(0x1008,1);put(0x100c,0);put(0x1010,0);put(0x1014,0);put(0x3010,0);put(0x3014,0);put(0x3018,0);
            float radius=1.7320507764816284f;put(0x301c,-(n%82==0?radius:nextafterf(radius,INFINITY)));
        }
        if(n%101==0){ /* An accepted traversal would be an incorrect fast path. */
            MEM32(0x1004)=0x4000;MEM32(0x3000)=1;put(0x1008,1);put(0x100c,0);put(0x1010,0);put(0x1014,0);put(0x3010,1);put(0x3014,0);put(0x3018,0);put(0x301c,100);
            for(unsigned i=0;i<8;++i){uint32_t node=0x4000+i*24;MEM32(node+4)=0;put(node+8,1);put(node+12,0);put(node+16,0);put(node+20,0);}
        }
        if(n%303==0){MEM32(0x4004)=0x5000;for(unsigned i=0;i<8;i++){uint32_t child=0x5000+i*24;MEM32(child+4)=0;put(child+8,1);put(child+12,0);put(child+16,0);put(child+20,0);}}
        if(n%43==0)MEM32(0x3000)=0;if(n%47==0)MEM32(0x3000)=0xfffffffeu;
        if(compare(n,initial,expected))++accelerated;else ++fallback;
    }
    /* Read-only source alias: node occupies plane0; it rejects before its
     * fields could be interpreted as a child traversal or callback. */
    registers(19);MEM32(0x644024)=0x3000;MEM32(0x3000)=1;g_ecx=0x3010;MEM32(0x3010)=0x3f800000;MEM32(0x3014)=0;put(0x3018,1);put(0x301c,-10);put(0x3020,0);put(0x3024,0);assert(compare(5000,initial,expected));++accelerated;
    /* Stack-alias guards must decline without even prologue/scratch writes. */
    for(unsigned i=0;i<3;++i){registers(i);if(i==0)g_ecx=g_esp-24;else if(i==1)MEM32(0x644024)=g_esp-32;else g_esp=0x644028;g_esi=g_ecx;g_esp-=40;State a={0},b={0};save(&a);memcpy(initial,(void*)g_mem_base,0x800000);assert(!ds_culling_prefix_50c359());save(&b);assert(!memcmp(&a,&b,sizeof a));assert(!memcmp(initial,(void*)g_mem_base,0x800000));}
    printf("Culling prefix routing: %u complete comparisons / %u failed; full 8MiB, registers/flags/FPU/shadows/traces, callback/traversal and stack-alias guards PASS\n",accelerated,fallback);
    }
#ifdef RECOMP_FP_STACK_EXACT
    puts("Benchmark reference: RECOMP_FP_STACK_EXACT=1 with out-of-line exact FP stack helpers");
#else
    puts("Benchmark reference: original portable fixed-window FP stack helpers");
#endif
    ds_culling_fast_control(0);{State a={0},b={0};save(&a);memcpy(initial,(void*)g_mem_base,0x800000);assert(!ds_culling_prefix_50c359());save(&b);assert(!memcmp(&a,&b,sizeof a));assert(!memcmp(initial,(void*)g_mem_base,0x800000));}ds_culling_fast_control(1);
    /* Relative host/device helper timing only; never a whole-game FPS claim. */
    registers(0);g_ecx=0x1000;MEM32(0x1004)=0;MEM32(0x644028)=0;MEM32(0x644024)=0x3000;MEM32(0x3000)=6;
    put(0x1008,1);put(0x100c,0);put(0x1010,0);put(0x1014,0);put(0x5dad3c,1.7320507764816284f);put(0x5d8344,0);
    for(int mode=0;mode<3;++mode){
        for(unsigned i=0;i<6;++i){put(0x3010+i*16,1);put(0x3014+i*16,0);put(0x3018+i*16,0);put(0x301c+i*16,100);}
        if(mode<2)put(0x301c+(mode?5:0)*16,-100);
        struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);double begin=t.tv_sec+t.tv_nsec/1e9;
        for(unsigned i=0;i<300000;++i){g_ecx=0x1000;g_esp=0xf000;reference_0050C350();}
        clock_gettime(CLOCK_MONOTONIC,&t);double ref=t.tv_sec+t.tv_nsec/1e9-begin;begin=t.tv_sec+t.tv_nsec/1e9;
        for(unsigned i=0;i<300000;++i){g_ecx=0x1000;g_esp=0xf000;candidate_0050C350();}
        clock_gettime(CLOCK_MONOTONIC,&t);double fast=t.tv_sec+t.tv_nsec/1e9-begin;
        printf("Helper-only %s: lifted %.6fs guarded %.6fs ratio %.3fx\n",mode==0?"first-plane reject":mode==1?"sixth-plane reject":"visible continuation",ref,fast,ref/fast);
    }
    free(initial);free(expected);free((void*)g_mem_base);return 0;
}
