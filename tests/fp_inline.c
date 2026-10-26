#define _POSIX_C_SOURCE 200809L
#include "recomp_types.h"
#include <assert.h>
#include <fenv.h>
#include <time.h>
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;ptrdiff_t g_mem_base;
extern void reference_00551A40(void),candidate_00551A40(void);
enum { BASE=0x5D0000, MEMORY=0x10000, STACK=BASE+0xe000, PLANES=BASE+0x1000, SPHERE=BASE+0x3000, RESULT=BASE+0x4000 };
typedef struct {uint32_t r[9],flags[4];uint64_t st[8],shadow[8];int top;uint16_t cw;} State;
static void save(State*s) {
    memset(s,0,sizeof *s);
    uint32_t r[]={g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func};
    uint32_t f[]={g_flag_k,g_flag_a,g_flag_b,g_flag_cf};
    memcpy(s->r,r,sizeof r);memcpy(s->flags,f,sizeof f);memcpy(s->st,g_st,64);memcpy(s->shadow,g_st_i64,64);s->top=g_fp_top;s->cw=g_fpu_cw;
}
static void restore(const State*s) {
    g_eax=s->r[0];g_ecx=s->r[1];g_edx=s->r[2];g_esp=s->r[3];g_ebx=s->r[4];g_esi=s->r[5];g_edi=s->r[6];g_ebp=s->r[7];g_cur_func=s->r[8];
    g_flag_k=s->flags[0];g_flag_a=s->flags[1];g_flag_b=s->flags[2];g_flag_cf=s->flags[3];
    memcpy(g_st,s->st,64);memcpy(g_st_i64,s->shadow,64);g_fp_top=s->top;g_fpu_cw=s->cw;
}
static uint32_t rng=0x551a4051;
static uint32_t random32(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static void put_float(uint32_t a,float v){memcpy((void*)ADDR(a),&v,4);}
int main(void) {
    uint8_t *mem=malloc(MEMORY),*initial=malloc(MEMORY),*expected=malloc(MEMORY);assert(mem&&initial&&expected);g_mem_base=(ptrdiff_t)mem-BASE;
    State start,want,got;
    const int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    const uint32_t edge[]={0,0x80000000,1,0x80000001,0x007fffff,0x00800000,0x7f7fffff,0xff7fffff,0x3f800000,0xbf800000,0x7fc12345,0x7f812345,0x7f800000,0xff800000};
    unsigned accepted=0,rejected=0,empty=0;
    for(unsigned n=0;n<24000;n++) {
        assert(!fesetround(modes[n%4]));
        for(unsigned i=0;i<MEMORY;i+=4)MEM32(BASE+i)=random32();
        uint32_t planes=PLANES+(n%13==0),sphere=SPHERE+(n%17==0),result=(n%5)?RESULT:0;
        // Output may alias source coefficients; exact ordered memory semantics.
        if(n%19==0)result=planes+16;
        if(n%23==0)result=sphere;
        int count=(int)(n%18)-1;MEM32(planes)=(uint32_t)count;
        for(unsigned i=0;i<18*4;i++) {
            uint32_t a=planes+16+4*i;
            if(n%3==0)MEM32(a)=edge[(n/3+i)%14];
            else if(n%3==1)put_float(a,((int)(random32()%2001)-1000)/32.0f);
        }
        for(unsigned i=0;i<4;i++) {
            if(n%3==0)MEM32(sphere+4*i)=edge[(n/3+i+7)%14];
            else if(n%3==1)put_float(sphere+4*i,((int)(random32()%2001)-1000)/16.0f);
        }
        put_float(0x5D99E0,-0.00001f);put_float(0x5D8C44,0.00001f);
        if(n%29==0)MEM32(0x5D99E0)=edge[(n/29)%14];
        if(n%31==0)MEM32(0x5D8C44)=edge[(n/31)%14];
        g_eax=random32();g_ecx=planes;g_edx=random32();g_esp=STACK;g_ebx=random32();g_esi=random32();g_edi=random32();g_ebp=random32();g_cur_func=0;
        g_flag_k=FK_ADD;g_flag_a=random32();g_flag_b=random32();g_flag_cf=n&1;
        for(unsigned i=0;i<8;i++){uint64_t v=((uint64_t)random32()<<32)|random32();memcpy(g_st+i,&v,8);g_st_i64[i]=(int64_t)(((uint64_t)random32()<<32)|random32());}
        g_fp_top=(int)(n%12)-2;g_fpu_cw=0x37f|((n%4)<<10);
        MEM32(STACK)=RECOMP_RETADDR;MEM32(STACK+4)=sphere;MEM32(STACK+8)=result;
        save(&start);memcpy(initial,mem,MEMORY);feclearexcept(FE_ALL_EXCEPT);
        reference_00551A40();int exwant=fetestexcept(FE_ALL_EXCEPT);save(&want);memcpy(expected,mem,MEMORY);
        restore(&start);memcpy(mem,initial,MEMORY);feclearexcept(FE_ALL_EXCEPT);
        candidate_00551A40();int exgot=fetestexcept(FE_ALL_EXCEPT);save(&got);
        if(memcmp(&want,&got,sizeof want)||memcmp(expected,mem,MEMORY)||exwant!=exgot) {
            fprintf(stderr,"Mismatch case%u rounding%d exceptions%#x/%#x\n",n,modes[n%4],exwant,exgot);
            for(unsigned i=0;i<sizeof want;i++)if(((uint8_t*)&want)[i]!=((uint8_t*)&got)[i]){fprintf(stderr,"state byte%u %02x/%02x\n",i,((uint8_t*)&want)[i],((uint8_t*)&got)[i]);break;}
            for(unsigned i=0;i<MEMORY;i++)if(expected[i]!=mem[i]){fprintf(stderr,"memory%#x %02x/%02x\n",BASE+i,expected[i],mem[i]);break;}
            return 1;
        }
        if(count<=0)empty++;else if(g_eax)accepted++;else rejected++;
    }
    assert(accepted&&rejected&&empty);
    printf("FP inline full-function PASS: 24000 cases, %u accepted/%u rejected/%u empty; all guest state, 64KiB memory, four rounding modes, FP exceptions, NaN/Inf, aliases and unaligned operands\n",accepted,rejected,empty);
    assert(!fesetround(FE_TONEAREST));memset(mem,0,MEMORY);MEM32(PLANES)=6;
    for(unsigned p=0;p<6;p++){put_float(PLANES+16+p*16,0.25f);put_float(PLANES+20+p*16,-0.5f);put_float(PLANES+24+p*16,0.75f);put_float(PLANES+28+p*16,1.f);}
    put_float(SPHERE,1.5f);put_float(SPHERE+4,2.f);put_float(SPHERE+8,3.f);put_float(SPHERE+12,100.f);
    put_float(0x5D99E0,-0.00001f);put_float(0x5D8C44,0.00001f);
    const unsigned loops=200000;double elapsed[2];
    for(unsigned k=0;k<2;k++) {
        double begin=seconds();
        for(unsigned i=0;i<loops;i++){g_ecx=PLANES;g_esp=STACK;g_fp_top=0;MEM32(STACK+4)=SPHERE;MEM32(STACK+8)=RESULT;if(k)candidate_00551A40();else reference_00551A40();}
        elapsed[k]=seconds()-begin;
    }
    printf("Plane routine %u calls: original=%.6fs inline=%.6fs ratio=%.3fx (not gameplay FPS)\n",loops,elapsed[0],elapsed[1],elapsed[0]/elapsed[1]);
    free(mem);free(initial);free(expected);return 0;
}
