#define _POSIX_C_SOURCE 200809L
#include "recomp_types.h"
#include "fp_stack_exact.h"
#if defined(DS_FP_TEST_INLINE)
#include "fp_stack_inline.h"
#endif
#include <assert.h>
#include <limits.h>
#include <time.h>
double g_st[8];int g_fp_top;
static uint64_t rng=0x35b9d29118726131ull;
static uint64_t random64(void){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return rng;}
typedef struct {uint64_t bits[8],shadow[8];int top;} Snapshot;
static void save(Snapshot *s){memset(s,0,sizeof *s);memcpy(s->bits,g_st,64);memcpy(s->shadow,g_st_i64,64);s->top=g_fp_top;}
static void restore(const Snapshot *s){memcpy(g_st,s->bits,64);memcpy(g_st_i64,s->shadow,64);g_fp_top=s->top;}
static uint64_t bits(double value){uint64_t r;memcpy(&r,&value,8);return r;}
#if defined(__GNUC__)
#define ISOLATED __attribute__((noinline,noclone))
#else
#define ISOLATED
#endif
static ISOLATED void reference_push(double value){fp_push_impl(g_st,&g_fp_top,value);}
static ISOLATED double reference_pop(void){return fp_pop_impl(g_st,&g_fp_top);}
#if defined(DS_FP_TEST_MACROS)
#define _st g_st
#define _fp_top g_fp_top
static ISOLATED void candidate_push(double value){fp_push(value);}
static ISOLATED double candidate_pop(void){return fp_pop();}
#else
static ISOLATED void candidate_push(double value){ds_fp_stack_push_exact(g_st,g_st_i64,&g_fp_top,value);}
static ISOLATED double candidate_pop(void){return ds_fp_stack_pop_exact(g_st,g_st_i64,&g_fp_top);}
#endif
static void equal(const Snapshot *a,const Snapshot *b,unsigned n){
    if(memcmp(a,b,sizeof *a)){fprintf(stderr,"FP stack state mismatch case%u\n",n);exit(1);}
}
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(void){
    static const uint64_t edges[]={0,0x8000000000000000ull,1,0x8000000000000001ull,0x000fffffffffffffull,0x0010000000000000ull,
       0x7fefffffffffffffull,0xffefffffffffffffull,0x7ff0000000000000ull,0xfff0000000000000ull,
       0x7ff8000000001234ull,0xfff8000000005678ull,0x7ff0000000004321ull,0xfff0000000008765ull,0xffffffffffffffffull};
    Snapshot initial,want,got;
    for(unsigned n=0;n<100000;n++){
        for(unsigned i=0;i<8;i++){uint64_t x=n%3?random64():edges[(n/3+i)%15];memcpy(&g_st[i],&x,8);g_st_i64[i]=(int64_t)random64();}
        g_fp_top=(int)(n%201)-100;if(n%257==0)g_fp_top=INT_MAX-256;if(n%263==0)g_fp_top=INT_MIN+256;
        save(&initial);uint64_t input=n%2?random64():edges[n%15];double value;memcpy(&value,&input,8);
        if(n%2){reference_push(value);save(&want);restore(&initial);candidate_push(value);save(&got);}
        else{uint64_t expected=bits(reference_pop());save(&want);restore(&initial);uint64_t actual=bits(candidate_pop());assert(expected==actual);save(&got);}
        equal(&want,&got,n);
    }
    /* Realistic nested mixes retain exact inactive-slot destruction too. */
    for(unsigned n=0;n<2000;n++){
        for(unsigned i=0;i<8;i++){uint64_t x=random64();memcpy(&g_st[i],&x,8);g_st_i64[i]=(int64_t)random64();}g_fp_top=(int)(n%10);save(&initial);
        uint64_t values[128],returns[128];unsigned ops[128];for(unsigned i=0;i<128;i++){values[i]=random64();ops[i]=(unsigned)(random64()&1);}
        for(unsigned i=0;i<128;i++){double v;memcpy(&v,&values[i],8);if(ops[i])reference_push(v);else returns[i]=bits(reference_pop());}save(&want);restore(&initial);
        for(unsigned i=0;i<128;i++){double v;memcpy(&v,&values[i],8);if(ops[i])candidate_push(v);else assert(returns[i]==bits(candidate_pop()));}save(&got);equal(&want,&got,n+100000);
    }
    puts("PASS:100000 raw-bit push/pop cases +2000x128 mixed operations; all8 double/shadow slots, inactive bits, signed zeros, infinities, quiet/signaling NaN payloads, top and pop return bits");
    const unsigned iterations=5000000;uint64_t checksum=0;g_fp_top=0;
    double t=seconds();for(unsigned i=0;i<iterations;i++){double x=(double)i;reference_push(x);checksum^=bits(reference_pop());}double original=seconds()-t;
    t=seconds();for(unsigned i=0;i<iterations;i++){double x=(double)i;candidate_push(x);checksum^=bits(candidate_pop());}double candidate=seconds()-t;
    printf("Kernel-only %u push/pop pairs: original=%.6fs candidate=%.6fs ratio=%.3fx checksum=%#llx (not game FPS)\n",iterations,original,candidate,original/candidate,(unsigned long long)checksum);
    return 0;
}
