/* Isolated CPU skinning wrapper. Fuses the already differential-tested scalar
 * matrix formulas and exactly reproduces original guest scratch writes. */
#include "recomp_types.h"
#include "skinning_fast.h"
#ifdef __FAST_MATH__
#error Original skinning rounding must not be reassociated
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC push_options
#pragma GCC optimize ("fp-contract=off")
#endif
static int enabled(void){static int initialized,value;if(!initialized){const char*s=getenv("DS_FAST_MATH");value=s&&!strcmp(s,"1");initialized=1;}return value;}
static int overlaps(uint32_t a,uint32_t an,uint32_t b,uint32_t bn){return(uint64_t)a+an>b&&(uint64_t)b+bn>a;}
static int available(uint32_t p,uint32_t n,uint32_t out,uint32_t bytes,uint32_t stack){
    return p<=UINT32_MAX-n&&!overlaps(p,n,out,bytes)&&!overlaps(p,n,stack-156,172);
}
static float read_float(uint32_t p){float f;memcpy(&f,(void*)ADDR(p),4);return f;}
static void write_float(uint32_t p,float f){memcpy((void*)ADDR(p),&f,4);}
static uint32_t bone_matrix(uint32_t influence,uint32_t palette,uint32_t metadata){
    uint32_t bone=MEM16(influence+36),index=MEM32(metadata+bone*136+132);return palette+index*68;
}
static void transform(uint32_t frame,uint32_t matrix,uint32_t src,uint32_t dst,int point){
    MEM32(frame-4)=dst;MEM32(frame-8)=src;MEM32(frame-12)=RECOMP_RETADDR;
    float v[3],m[16];memcpy(v,(void*)ADDR(src),12);memcpy(m,(void*)ADDR(matrix),64);
    /* Same exact reduction order as validated540470/5405A0, fused here to
     * avoid repeated per-vector dispatch/validation and global FPU moves. */
    double x=(double)v[2]*(double)m[8];double term=(double)v[1]*(double)m[4];x=x+term;
    term=(double)v[0]*(double)m[0];x=x+term;if(point)x=x+(double)m[12];
    double y=(double)v[0]*(double)m[1];term=(double)v[2]*(double)m[9];y=y+term;
    term=(double)v[1]*(double)m[5];y=y+term;if(point)y=y+(double)m[13];
    double z=(double)v[0]*(double)m[2];term=(double)v[2]*(double)m[10];z=z+term;
    term=(double)v[1]*(double)m[6];z=z+term;if(point)z=z+(double)m[14];
    float result[3]={(float)x,(float)y,(float)z};
    memcpy((void*)ADDR(frame-24),result,12);memcpy((void*)ADDR(dst),result,12);
    g_st[6]=g_st[7]=0.;g_st_i64[6]=g_st_i64[7]=0;
    uint32_t bits[3];memcpy(bits,result,12);
    g_eax=point?bits[1]:dst;g_ecx=point?dst:bits[2];g_edx=point?bits[2]:bits[1];
    g_esp=frame;g_cur_func=0x529520;
}

int ds_skinning_fast_529520(void){
    if(!enabled())return 0;
    uint32_t s=g_esp,self=g_ecx;
    if(s<156||s>UINT32_MAX-16u||self>UINT32_MAX-4u)return 0;
    uint32_t palette=MEM32(s+4),metadata=MEM32(s+8),out=MEM32(s+12),desc=MEM32(self);
    if(desc>UINT32_MAX-24u)return 0;
    uint32_t count=MEM32(desc+16),records=MEM32(desc+20);
    if(!count||count>65536u||out>UINT32_MAX-count*36u||records>UINT32_MAX-count*8u)return 0;
    uint32_t bytes=count*36u;
    if(overlaps(out,bytes,s-156,172)||!available(self,4,out,bytes,s)||!available(desc,24,out,bytes,s)||!available(records,count*8,out,bytes,s))return 0;
    /* Validate the complete common case first: no partial fallback after writes.
     * Normal game input pointers must already refer to readable guest memory. */
    /* Matrices cannot change during this call: every candidate matrix is
     * checked against output and guest scratch ranges before consulting this
     * cache, and this native path makes no guest callbacks. Keys are local to
     * one invocation; +1 encodes address zero without a sentinel collision. */
    uint64_t checked_matrix[32]={0};
    for(uint32_t i=0;i<count;i++){
        uint32_t n=MEM32(records+i*8),source=MEM32(records+i*8+4);
        if(!n||n>8u||!available(source,n*44,out,bytes,s))return 0;
        for(uint32_t j=0;j<n;j++){
            uint32_t p=source+j*44,bone=MEM16(p+36);
            uint64_t metadata_record=(uint64_t)metadata+bone*136u;
            if(metadata_record>UINT32_MAX-136u||!available((uint32_t)metadata_record,136,out,bytes,s))return 0;
            uint32_t index=MEM32((uint32_t)metadata_record+132);
            uint64_t address=(uint64_t)palette+index*68ull;
            if(address>UINT32_MAX-68u||!available((uint32_t)address,68,out,bytes,s))return 0;
            uint32_t m=(uint32_t)address,slot=(m>>2)&31u;
            uint64_t key=(uint64_t)m+1;
            if(checked_matrix[slot]!=key){
                if(!(MEM32(m+64)&2u))return 0;
                for(unsigned k=0;k<16;k++)if(!isfinite(read_float(m+k*4)))return 0;
                checked_matrix[slot]=key;
            }
            for(unsigned k=0;k<6;k++)if(!isfinite(read_float(p+k*4)))return 0;
            if(n>1&&!isfinite(read_float(p+40)))return 0;
        }
    }
    uint32_t frame=s-132;
    MEM32(s-120)=g_ebx;MEM32(s-124)=g_ebp;MEM32(s-100)=self;
    MEM32(s-128)=g_esi;MEM32(s-104)=0;MEM32(s-132)=g_edi;g_cur_func=0x529520;
    for(uint32_t i=0;i<count;i++){
        uint32_t record=records+i*8,n=MEM32(record),source=MEM32(record+4),dst=out+i*36,m;
        MEM32(frame+20)=record;
        if(n==1){
            for(unsigned k=0;k<3;k++){MEM32(frame+72+k*4)=MEM32(source+k*4);MEM32(frame+48+k*4)=MEM32(source+k*4);}
            for(unsigned k=0;k<3;k++){MEM32(frame+84+k*4)=MEM32(source+12+k*4);MEM32(frame+60+k*4)=MEM32(source+12+k*4);}
            m=bone_matrix(source,palette,metadata);
            transform(frame,m,frame+48,dst,1);
        }else{
            MEM32(frame+96)=0;MEM32(frame+100)=0;MEM32(frame+104)=0;
            MEM32(dst)=MEM32(dst+4)=MEM32(dst+8)=0;
            MEM32(frame+16)=(n-1)*44;MEM32(frame+24)=n;
            for(uint32_t j=n;j-->0;){
                uint32_t p=source+j*44;m=bone_matrix(p,palette,metadata);
                for(unsigned k=0;k<3;k++){MEM32(frame+108+k*4)=MEM32(p+k*4);MEM32(frame+48+k*4)=MEM32(p+k*4);}
                transform(frame,m,frame+48,frame+36,1);
                MEM32(frame+16)=j*44-44;MEM32(frame+24)=j;
                float weight=read_float(p+40);
                for(unsigned k=0;k<3;k++)write_float(frame+36+k*4,(float)((double)read_float(frame+36+k*4)*(double)weight));
                for(unsigned k=0;k<3;k++)write_float(dst+k*4,(float)((double)read_float(frame+36+k*4)+(double)read_float(dst+k*4)));
            }
            for(unsigned k=0;k<3;k++){MEM32(frame+120+k*4)=MEM32(source+12+k*4);MEM32(frame+60+k*4)=MEM32(source+12+k*4);}
            m=bone_matrix(source,palette,metadata);
        }
        transform(frame,m,frame+60,dst+12,0);
        MEM32(dst+24)=UINT32_MAX;MEM32(frame+28)=i+1;
    }
    g_eax=desc;g_esp=s+16;g_cur_func=0x529520;
    g_flag_k=FK_ADD;g_flag_a=s-116;g_flag_b=116;g_flag_cf=0;
    return 1;
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif
