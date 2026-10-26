/* Compare the complete row region, including inactive FP slots and aliases. */
#include "recomp_types.h"
#include "physics_product.h"
#include <assert.h>
#include <fenv.h>
#include <limits.h>
#include <time.h>
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func;
double g_st[8]; int g_fp_top; uint16_t g_fpu_cw; ptrdiff_t g_mem_base;
extern void reference_product(void);
extern int native_product(void);
typedef struct { uint32_t r[9],f[4]; double st[8]; int64_t sh[8]; int top; uint16_t cw; } State;
static void save(State *s) {
    memset(s,0,sizeof *s);
    uint32_t r[]={g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func};
    uint32_t f[]={g_flag_k,g_flag_a,g_flag_b,g_flag_cf};
    memcpy(s->r,r,sizeof r); memcpy(s->f,f,sizeof f);
    memcpy(s->st,g_st,sizeof g_st); memcpy(s->sh,g_st_i64,sizeof g_st_i64);
    s->top=g_fp_top; s->cw=g_fpu_cw;
}
static void restore(const State *s) {
    g_eax=s->r[0];g_ecx=s->r[1];g_edx=s->r[2];g_esp=s->r[3];g_ebx=s->r[4];
    g_esi=s->r[5];g_edi=s->r[6];g_ebp=s->r[7];g_cur_func=s->r[8];
    g_flag_k=s->f[0];g_flag_a=s->f[1];g_flag_b=s->f[2];g_flag_cf=s->f[3];
    memcpy(g_st,s->st,sizeof g_st);memcpy(g_st_i64,s->sh,sizeof g_st_i64);
    g_fp_top=s->top;g_fpu_cw=s->cw;
}
static uint32_t seed=0x595daa;
static uint32_t random32(void) { seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed; }
static uint32_t finite32(void) { uint32_t n=random32();if((n&0x7f800000)==0x7f800000)n^=0x00800000;return n; }
static double seconds(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9; }
static void check(const State *want,const State *got,const uint8_t *mem,unsigned n) {
    if(memcmp(want,got,sizeof *want)||memcmp(mem,(void*)g_mem_base,65536)) {
        fprintf(stderr,"Product mismatch case %u\n",n);abort();
    }
}
int main(int argc,char **argv) {
    (void)argv;
    g_mem_base=(ptrdiff_t)calloc(1,65536);assert(g_mem_base);
    uint8_t initial[65536],expected[65536];State start,want,got;
    unsigned accepted=0,declined=0;
    if(argc>1) {
        g_ebp=g_esi=g_edi=g_ebx=UINT32_MAX;
        save(&start);memcpy(initial,(void*)g_mem_base,65536);
        assert(!native_product());save(&got);check(&start,&got,initial,0);
        puts("Product disabled: no guest access or mutation PASS");return 0;
    }
    static const uint32_t edges[]={0,0x80000000,1,0x80000001,0x7fffff,0x800000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f812345};
    for(unsigned n=0;n<20000;n++) {
        assert(!fesetround((int[]){FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}[n%4]));
        for(unsigned i=0;i<65536;i+=4)MEM32(i)=random32();
        uint32_t a=0x1000,b=0x2000,dst=0x3000,index=(n%4)*16;
        switch((n/4)%8) {
            case 0:dst=a;break;case 1:dst=b;break;case 2:dst=a+4;break;
            case 3:dst=b-4;break;case 4:b=a;break;case 5:b=a+4;break;
            case 6:a++;b++;dst++;break;
        }
        for(unsigned i=0;i<16;i++) { MEM32(a+4*i)=finite32();MEM32(b+4*i)=finite32(); }
        for(unsigned i=0;i<4;i++)MEM32(dst+4*i)=finite32();
        if(n%13==0)for(unsigned i=0;i<16;i++)MEM32(b+4*i)=edges[(n/13+i)%12];
        if(n%17==0)MEM32(a)=edges[(n/17)%12];
        if(n%19==0)MEM32(dst)=edges[(n/19)%12];
        g_eax=random32();g_ecx=random32();g_edx=random32();g_esp=0xf000;g_ebx=dst;
        g_esi=a-index*4;g_edi=b-index*4;g_ebp=0xe000;g_cur_func=random32();
        if(n%23==0)g_ebp=dst+12;
        MEM32(g_ebp-12)=index;
        g_flag_k=random32();g_flag_a=random32();g_flag_b=random32();g_flag_cf=random32();
        for(unsigned i=0;i<8;i++) {
            uint64_t bits=((uint64_t)random32()<<32)|random32();memcpy(g_st+i,&bits,8);
            g_st_i64[i]=(int64_t)(((uint64_t)random32()<<32)|random32());
        }
        g_fp_top=(int)(n%11)-4;g_fpu_cw=(uint16_t)random32();
        save(&start);memcpy(initial,(void*)g_mem_base,65536);
        reference_product();save(&want);memcpy(expected,(void*)g_mem_base,65536);
        restore(&start);memcpy((void*)g_mem_base,initial,65536);
        if(native_product())accepted++;
        else { declined++;save(&got);check(&start,&got,initial,n);reference_product(); }
        save(&got);check(&want,&got,expected,n);
    }
    for(unsigned n=0;n<7;n++) {
        g_ebp=0xe000;g_esi=0x1000;g_edi=0x2000;g_ebx=0x3000;g_fp_top=0;MEM32(g_ebp-12)=0;
        if(n==0)g_ebp=0;if(n==1)g_ebp=UINT32_MAX;
        if(n==2)g_esi=UINT32_MAX-16;if(n==3)g_edi=UINT32_MAX-16;
        if(n==4)g_ebx=UINT32_MAX-4;if(n==5)MEM32(g_ebp-12)=UINT32_MAX;
        if(n==6)g_fp_top=INT_MAX;
        save(&want);memcpy(expected,(void*)g_mem_base,65536);
        assert(!native_product());save(&got);check(&want,&got,expected,n);
    }
    assert(accepted>10000&&declined>1000);
    printf("Product 20000 cases PASS: accepted=%u fallback=%u; full memory/GPR/flags/rawFP/shadows/top/CW; aliases, four rounding modes, NaN/inf/bounds\n",accepted,declined);
    assert(!fesetround(FE_TONEAREST));
    g_esi=0x1000;g_edi=0x2000;g_ebx=0x3000;g_ebp=0xe000;g_fp_top=0;MEM32(g_ebp-12)=0;
    for(unsigned i=0;i<16;i++) { float v=(i+1)/64.f;memcpy((void*)ADDR(g_esi+i*4),&v,4);memcpy((void*)ADDR(g_edi+i*4),&v,4); }
    double elapsed[2];unsigned loops=200000;
    for(unsigned k=0;k<2;k++) {
        double start_time=seconds();
        for(unsigned i=0;i<loops;i++) { memset((void*)ADDR(g_ebx),0,16);if(k)assert(native_product());else reference_product(); }
        elapsed[k]=seconds()-start_time;
    }
    printf("Product kernel %u calls: reference=%.6fs scalar=%.6fs ratio=%.3fx; not gameplay FPS\n",loops,elapsed[0],elapsed[1],elapsed[0]/elapsed[1]);
    free((void*)g_mem_base);return 0;
}
