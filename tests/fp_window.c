/* RECOMP_FP_WINDOW differential test: random x87 stack traffic through the
 * generated-code macros must match the original shifting model bit for bit,
 * including inactive slots, integer shadows, NaN payloads and recentering. */
#define RECOMP_GENERATED_CODE
#define RECOMP_FP_WINDOW 1
#include "recomp_types.h"
#include <stdio.h>
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp;
int g_fp_top;uint16_t g_fpu_cw;ptrdiff_t g_mem_base;
static double rs[8];static int64_t rh[8];static int rtop;
static void rpush(double v){for(int i=7;i>0;i--){rs[i]=rs[i-1];rh[i]=rh[i-1];}rs[0]=v;rh[0]=0;rtop++;}
static double rpop(void){double v=rs[0];for(int i=0;i<7;i++){rs[i]=rs[i+1];rh[i]=rh[i+1];}uint64_t z=0;memcpy(&rs[7],&z,8);rh[7]=0;rtop--;return v;}
static uint64_t x=88172645463325252ull;
static uint64_t rnd(void){x^=x<<13;x^=x>>7;x^=x<<17;return x;}
static double rval(void){uint64_t b=rnd();if(rnd()%4==0)b=0x7ff4000000000000ull|(rnd()&0xffff);double d;memcpy(&d,&b,8);return d;}
int main(void){
    for(long it=0;it<4000000;it++){
        unsigned op=rnd()%7;int i=(int)(rnd()%8);
        /* Long push-heavy and pop-heavy phases force recentering both ways. */
        if((it/200000)%3==1)op=(rnd()%10<8)?0:op;else if((it/200000)%3==2)op=(rnd()%10<8)?1:op;
        switch(op){
        case 0:{double v=rval();rpush(v);fp_push(v);break;}
        case 1:{double a=rpop(),b=fp_pop();if(memcmp(&a,&b,8)){printf("FAIL pop at %ld\n",it);return 1;}break;}
        case 2:{double t=rs[0];rs[0]=rs[i];rs[i]=t;int64_t u=rh[0];rh[0]=rh[i];rh[i]=u;fp_xch(i);break;}
        case 3:{double v=rval();rs[i]=v;_st[i]=v;break;}
        case 4:{int64_t v=(int64_t)rnd();rpush((double)v);rh[0]=v;fp_push_i64(v);break;}
        case 5:{int64_t v=(int64_t)rnd();rh[i]=v;g_st_i64[i]=v;break;}
        case 6:{double v=_st[i]*1.5+_st[(i+1)&7],w=rs[i]*1.5+rs[(i+1)&7];rpush(w);fp_push(v);break;}
        }
        if(memcmp(rs,&_st[0],64)||memcmp(rh,&g_st_i64[0],64)||rtop!=_fp_top){printf("FAIL state at %ld op %u\n",it,op);return 1;}
    }
    puts("PASS: 4000000 x87 window operations match the shifting model");
    return 0;
}
