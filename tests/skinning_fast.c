#define _POSIX_C_SOURCE 200809L
#include "recomp_types.h"
#include "skinning_fast.h"
#include <assert.h>
#include <fenv.h>
#include <time.h>
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;ptrdiff_t g_mem_base;
extern void reference_00529520(void),current_00529520(void);
enum {MEMORY=0x40000,STACK=0x30000};
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
static void set_float(uint32_t p,float f){memcpy((void*)ADDR(p),&f,4);}
static void check(const State *want,const State *got,const uint8_t *memory,unsigned n,int fast){
    if(!memcmp(want,got,sizeof *want)&&!memcmp(memory,(void*)g_mem_base,MEMORY))return;
    fprintf(stderr,"Skin mismatch case=%u fast=%d\n",n,fast);
    for(unsigned i=0;i<sizeof *want;i++)if(((const uint8_t*)want)[i]!=((const uint8_t*)got)[i]){fprintf(stderr,"state byte%u want%02x got%02x\n",i,((const uint8_t*)want)[i],((const uint8_t*)got)[i]);break;}
    for(unsigned i=0;i<MEMORY;i++)if(memory[i]!=((const uint8_t*)g_mem_base)[i]){fprintf(stderr,"memory%#x want%02x got%02x\n",i,memory[i],((const uint8_t*)g_mem_base)[i]);break;}
    for(unsigned i=0;i<9;i++)fprintf(stderr,"reg%u want%#x got%#x\n",i,want->regs[i],got->regs[i]);
    exit(1);
}
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(int argc,char**argv){
    g_mem_base=(ptrdiff_t)malloc(MEMORY);assert(g_mem_base);
    uint8_t *initial=malloc(MEMORY),*expected=malloc(MEMORY);assert(initial&&expected);
    State start,want,got;unsigned accepted=0,declined=0;
    const uint32_t self=0x1000,desc=0x1100,records=0x2000,out=0x4000,palette=0x8000,metadata=0xa000;
    static const int rounding[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    for(unsigned n=0;n<12000;n++){
        assert(!fesetround(rounding[(n/12)%4]));
        for(unsigned i=0;i<MEMORY;i+=4){uint32_t x=random32();memcpy((void*)ADDR(i),&x,4);}
        uint32_t count=n%12+1;MEM32(self)=desc;MEM32(desc+16)=count;MEM32(desc+20)=records;
        for(unsigned i=0;i<8;i++){
            for(unsigned k=0;k<16;k++)set_float(palette+i*68+k*4,(float)(int32_t)random32()/32768.f);
            MEM32(palette+i*68+64)=2+(i%2);MEM32(metadata+i*136+132)=i;
        }
        for(unsigned i=0;i<count;i++){
            uint32_t influences=(n+i)%4+1;if(n%17==0)influences=8;
            uint32_t source=0xc000+i*0x200;MEM32(records+i*8)=influences;MEM32(records+i*8+4)=source;
            for(unsigned j=0;j<influences;j++){
                for(unsigned k=0;k<6;k++)set_float(source+j*44+k*4,(float)(int32_t)random32()/4096.f);
                MEM16(source+j*44+36)=(uint16_t)((i+j)%8);
                set_float(source+j*44+40,(float)((int)(random32()%17)-8)/4.f);
            }
        }
        if(n%29==0)MEM32(0xc000)=0x7fc01234; /* unchanged fallback */
        if(n%31==0)MEM32(palette+64)=0;      /* mode fallback */
        if(n%37==0){MEM32(0xc000)=0x7f7fffff;MEM32(0xc004)=0xff7fffff;set_float(palette,2.f);}
        g_eax=random32();g_ecx=self;g_edx=random32();g_esp=STACK;g_ebx=random32();g_esi=random32();g_edi=random32();g_ebp=random32();g_cur_func=random32();
        g_flag_k=random32();g_flag_a=random32();g_flag_b=random32();g_flag_cf=random32();
        for(unsigned i=0;i<8;i++){g_st[i]=(double)(int32_t)random32()/256.;g_st_i64[i]=(int64_t)(((uint64_t)random32()<<32)|random32());}
        g_fp_top=(int)(n%11)-2;g_fpu_cw=n%2?0x37f:0x27f;
        MEM32(STACK)=RECOMP_RETADDR;MEM32(STACK+4)=palette;MEM32(STACK+8)=metadata;MEM32(STACK+12)=out;
        save(&start);memcpy(initial,(void*)g_mem_base,MEMORY);
        if(argc>1&&!strcmp(argv[1],"--disabled")){assert(!ds_skinning_fast_529520());save(&got);check(&start,&got,initial,n,0);puts("Skin opt-in disabled: no mutation PASS");return 0;}
        reference_00529520();save(&want);memcpy(expected,(void*)g_mem_base,MEMORY);
        restore(&start);memcpy((void*)g_mem_base,initial,MEMORY);
        int fast=ds_skinning_fast_529520();
        if(!fast){save(&got);check(&start,&got,initial,n,0);declined++;reference_00529520();}else accepted++;
        save(&got);check(&want,&got,expected,n,fast);
    }
    assert(accepted>10000);printf("Skinning %u accelerated/%u unchanged fallback: full memory/registers/flags/FPU PASS\n",accepted,declined);
    /* Structured rejection checks do not execute malformed original pointers. */
    assert(!fesetround(FE_TONEAREST));
    MEM32(self)=desc;MEM32(desc+16)=1;MEM32(desc+20)=records;MEM32(records)=1;MEM32(records+4)=0xc000;
    for(unsigned k=0;k<16;k++)set_float(palette+k*4,(k%5==0)?1.f:0.f);MEM32(palette+64)=2;
    for(unsigned k=0;k<6;k++)set_float(0xc000+k*4,(float)(k+1));MEM16(0xc000+36)=0;MEM32(metadata+132)=0;
    for(unsigned n=0;n<7;n++){
        g_ecx=self;g_esp=STACK;MEM32(STACK+4)=palette;MEM32(STACK+8)=metadata;MEM32(STACK+12)=out;MEM32(desc+16)=1;MEM32(records)=1;
        if(n==0)MEM32(STACK+12)=0xc000;if(n==1)MEM32(STACK+12)=STACK-80;
        if(n==2)MEM32(records)=0;if(n==3)MEM32(records)=9;if(n==4)MEM32(desc+16)=0;
        if(n==5)g_esp=155;if(n==6)MEM32(STACK+12)=metadata;
        save(&start);memcpy(initial,(void*)g_mem_base,MEMORY);assert(!ds_skinning_fast_529520());save(&got);check(&start,&got,initial,n,0);
    }
    puts("Skinning alias/stack/count declines leave all state unchanged PASS");
    /* Incremental baseline: original wrapper with already-accelerated matrix
     * helpers; AArch64 build additionally enables exact FP stack macros. */
    MEM32(desc+16)=128;MEM32(desc+20)=records;
    for(unsigned m=0;m<8;m++){
        for(unsigned k=0;k<16;k++)set_float(palette+m*68+k*4,k%5==0?1.f:0.f);
        MEM32(palette+m*68+64)=2;MEM32(metadata+m*136+132)=m;
    }
    for(unsigned influences=1;influences<=4;influences*=2){
        for(unsigned i=0;i<128;i++){
            uint32_t source=0xc000+i*0x200;MEM32(records+i*8)=influences;MEM32(records+i*8+4)=source;
            for(unsigned j=0;j<influences;j++){
                for(unsigned k=0;k<6;k++)set_float(source+j*44+k*4,(float)(i+j+k)/4.f);
                MEM16(source+j*44+36)=(uint16_t)((i+j)%8);set_float(source+j*44+40,1.f/(float)influences);
            }
        }
        g_ecx=self;g_esp=STACK;MEM32(STACK+4)=palette;MEM32(STACK+8)=metadata;MEM32(STACK+12)=out;
        save(&start);memcpy(initial,(void*)g_mem_base,MEMORY);
        current_00529520();save(&want);memcpy(expected,(void*)g_mem_base,MEMORY);
        restore(&start);memcpy((void*)g_mem_base,initial,MEMORY);assert(ds_skinning_fast_529520());save(&got);check(&want,&got,expected,12000+influences,1);
        unsigned iterations=2000;double t=seconds();
        for(unsigned i=0;i<iterations;i++){g_ecx=self;g_esp=STACK;current_00529520();}double current=seconds()-t;
        t=seconds();for(unsigned i=0;i<iterations;i++){g_ecx=self;g_esp=STACK;assert(ds_skinning_fast_529520());}double native=seconds()-t;
        printf("Kernel-only %u vertices %u influences: current-math-accelerated=%.6fs fused=%.6fs ratio=%.3fx (not game FPS)\n",iterations*128,influences,current,native,current/native);
    }
    free(initial);free(expected);free((void*)g_mem_base);return 0;
}
