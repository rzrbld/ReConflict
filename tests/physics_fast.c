/* Differential ABI/memory/FPU test against unchanged lifted game routines. */
#include "recomp_types.h"
#include "physics_fast.h"

#include <assert.h>
#include <time.h>
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;
ptrdiff_t g_mem_base;
extern void reference_00595B10(void),native_00595B10(void),reference_00595B59(void),native_00595B59(void);
uint32_t g_icall_trace[ICALL_TRACE_SIZE],g_icall_from[ICALL_TRACE_SIZE],g_icall_trace_idx,g_icall_count;
int ds_capture;unsigned ds_accepted,ds_declined;
recomp_func_t recomp_lookup(uint32_t va){fprintf(stderr,"Unexpected profile dispatch %x\n",va);abort();}
recomp_func_t recomp_lookup_manual(uint32_t va){return recomp_lookup(va);}
recomp_func_t recomp_lookup_import(uint32_t va){return recomp_lookup(va);}
typedef struct{uint32_t regs[9],flags[4];double st[8];int64_t shadow[8];int top;uint16_t cw;} State;
static void save(State*s){uint32_t regs[]={g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_cur_func},flags[]={g_flag_k,g_flag_a,g_flag_b,g_flag_cf};memcpy(s->regs,regs,sizeof regs);memcpy(s->flags,flags,sizeof flags);memcpy(s->st,g_st,sizeof g_st);memcpy(s->shadow,g_st_i64,sizeof g_st_i64);s->top=g_fp_top;s->cw=g_fpu_cw;}
static void restore(const State*s){g_eax=s->regs[0];g_ecx=s->regs[1];g_edx=s->regs[2];g_esp=s->regs[3];g_ebx=s->regs[4];g_esi=s->regs[5];g_edi=s->regs[6];g_ebp=s->regs[7];g_cur_func=s->regs[8];g_flag_k=s->flags[0];g_flag_a=s->flags[1];g_flag_b=s->flags[2];g_flag_cf=s->flags[3];memcpy(g_st,s->st,sizeof g_st);memcpy(g_st_i64,s->shadow,sizeof g_st_i64);g_fp_top=s->top;g_fpu_cw=s->cw;}
static uint32_t rng=0x12345678;
static uint32_t random32(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static uint32_t finite32(void){uint32_t n=random32();if((n&0x7f800000)==0x7f800000)n^=0x00800000;return n;}
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(void){
 int disabled=!getenv("DS_FAST_MATH")||strcmp(getenv("DS_FAST_MATH"),"1");
 if(disabled){uint32_t e=g_ecx,si=g_esi,bx=g_ebx,d=g_edx;g_ecx=4;g_esi=g_ebx=UINT32_MAX;g_edx=UINT32_MAX;State a={0},b={0};save(&a);assert(!ds_physics_rank_update_eligible());save(&b);assert(!memcmp(&a,&b,sizeof a));g_ecx=e;g_esi=si;g_ebx=bx;g_edx=d;}
 const unsigned bytes=0x800000;g_mem_base=(ptrdiff_t)calloc(1,bytes);uint8_t *initial=malloc(bytes),*expected=malloc(bytes);assert(g_mem_base&&initial&&expected);State start={0},want={0},got={0};unsigned full=0,alias=0;
 for(unsigned n=0;n<240;++n){
  memset((void*)g_mem_base,0,bytes);unsigned blocks=n%7,dim=4*blocks;uint32_t object=0x1000,matrix=0x2000,recip=0x10000,presence=0x11000,last=0x12000,first=0x13000,stack=0x700000;
  MEM32(object+0x1c)=blocks;MEM32(object+0x20)=recip;MEM32(object+0x24)=matrix;MEM32(object+0x2c)=presence;MEM32(object+0x30)=last;MEM32(object+0x34)=first;
  double b[28][28],a[28][28];for(unsigned r=0;r<dim;r++)for(unsigned c=0;c<dim;c++)b[r][c]=(int32_t)(random32()%257)-128;
  for(unsigned r=0;r<dim;r++)for(unsigned c=0;c<dim;c++){double v=0;for(unsigned k=0;k<dim;k++)v+=b[k][r]*b[k][c]/65536.;a[r][c]=v+(r==c?dim+1:0);}
  for(unsigned br=0;br<blocks;br++)for(unsigned bc=0;bc<blocks;bc++)for(unsigned r=0;r<4;r++)for(unsigned c=0;c<4;c++){float v=(float)a[br*4+r][bc*4+c];if(n%8==0)v=r==c&&br==bc?0.f:0.f;if(n%8==1&&br==bc&&r==c)v=-v;if(n%8==2&&br==bc&&r==c)v=1e-30f;memcpy((void*)ADDR(matrix+64*(br*blocks+bc)+4*(r*4+c)),&v,4);}
  memset((void*)ADDR(presence),1,blocks*blocks);memset((void*)ADDR(last),blocks?blocks-1:0,blocks);memset((void*)ADDR(first),0,blocks);
  if(n%13==0&&blocks>1)MEM32(matrix+64)=0x7fc01234;if(n%17==0&&blocks>1)MEM32(matrix+68)=0x7f801234;if(n%19==0&&blocks>1)MEM32(matrix+72)=0x7f800000;
  if(n%11==0&&blocks>1)MEM8(presence+1)=0;
  g_eax=random32();g_ecx=object;g_edx=random32();g_esi=random32();g_esp=stack;g_ebx=random32();g_edi=random32();g_ebp=random32();g_cur_func=random32();g_flag_k=random32();g_flag_a=random32();g_flag_b=random32();g_flag_cf=random32();
  for(unsigned i=0;i<8;i++){uint64_t bits=((uint64_t)random32()<<32)|random32();memcpy(&g_st[i],&bits,8);g_st_i64[i]=((uint64_t)random32()<<32)|random32();}g_fp_top=(int)(n%9)-4;g_fpu_cw=(uint16_t)random32();MEM32(stack)=RECOMP_RETADDR;MEM8(0x77097c)=0;
  save(&start);memcpy(initial,(void*)g_mem_base,bytes);
  reference_00595B10();if(n%8>=3&&n%13&&n%17&&n%19){for(unsigned j=0;j<dim;j++){float v=*(float*)ADDR(recip+4*j);if(!isfinite(v)||!(v>0)){fprintf(stderr,"SPD fixture invalid n=%u j=%u value=%g\n",n,j,v);return 1;}}}save(&want);memcpy(expected,(void*)g_mem_base,bytes);
  restore(&start);memcpy((void*)g_mem_base,initial,bytes);native_00595B10();save(&got);
  if(memcmp(&want,&got,sizeof want)||memcmp(expected,(void*)g_mem_base,bytes)){fprintf(stderr,"Full mismatch n=%u blocks=%u\n",n,blocks);return 1;}++full;
  if(blocks>1){
   restore(&start);memcpy((void*)g_mem_base,initial,bytes);ds_capture=1;reference_00595B10();assert(!ds_capture);save(&start);memcpy(initial,(void*)g_mem_base,bytes);
   reference_00595B59();save(&want);memcpy(expected,(void*)g_mem_base,bytes);
   restore(&start);memcpy((void*)g_mem_base,initial,bytes);native_00595B59();save(&got);
   if(memcmp(&want,&got,sizeof want)||memcmp(expected,(void*)g_mem_base,bytes)){fprintf(stderr,"Alias mismatch n=%u blocks=%u\n",n,blocks);return 1;}++alias;
  }
 }
 printf("PASS full factorize=%u alias continuation=%u complete8MiB/GPR/flags/rawFP/shadows/depth/CW acceptedkernels=%u declinediterations=%u; original profiling-disabled callees and stack allocator\n",full,alias,ds_accepted,ds_declined);assert(disabled?(ds_accepted==0&&ds_declined>0):(ds_accepted>0&&ds_declined>0));printf("DS_FAST_MATH mode=%s PASS\n",disabled?"disabled":"enabled");free(initial);free(expected);free((void*)g_mem_base);return 0;
}
