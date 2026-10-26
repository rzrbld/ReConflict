#define _GNU_SOURCE
#include "index_scan.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
static uint32_t seed=0x20261027;
static unsigned random32(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
__attribute__((noinline))
static int reference(const void *indices,unsigned size,unsigned n,unsigned count,unsigned base,unsigned *lo,unsigned *hi) {
    if(!n)return 0;unsigned mn=UINT32_MAX,mx=0;
    for(unsigned i=0;i<n;i++) {
        unsigned ix=i;
        if(indices) {
            if(size==2){uint16_t v;memcpy(&v,(const char*)indices+i*2,2);ix=v;}
            else if(size==4)memcpy(&ix,(const char*)indices+i*4,4);
            else return 0;
        }
        if(ix>=count||ix>UINT32_MAX-base)return 0;
        if(ix<mn)mn=ix;if(ix>mx)mx=ix;
    }
    *lo=mn;*hi=mx;return 1;
}
__attribute__((noinline))
static int candidate(const void *p,unsigned size,unsigned n,unsigned count,unsigned base,size_t span,unsigned *lo,unsigned *hi){return ds_index_scan(p,size,n,count,base,span,lo,hi);}
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(void) {
    unsigned char *mem=malloc(300000*4+16);assert(mem);unsigned accepted=0,declined=0;
    for(unsigned z=0;z<24000;z++) {
        unsigned n=z<16?z:random32()%2048,size=z&1?2:4;
        unsigned char *p=mem+(z%8);unsigned count=z%3?2048:random32(),base=z%7?0:UINT32_MAX-1024;
        for(unsigned i=0;i<n;i++) { unsigned v=random32()%2048;if(z%11==0&&i==n/2)v=UINT32_MAX;memcpy(p+(size_t)i*size,&v,size); }
        const void *data=z%5?p:NULL;size_t span=(size_t)n*size;
        if(z%13==0&&span)span--;
        unsigned lo=0x12345678,hi=0x87654321,wlo=lo,whi=hi;
        int want=reference(data,size,n,count,base,&wlo,&whi);
        if(data&&span<(size_t)n*size)want=0;
        int got=candidate(data,size,n,count,base,span,&lo,&hi);
        assert(want==got);
        if(got){accepted++;assert(lo==wlo&&hi==whi);}else{declined++;assert(lo==0x12345678&&hi==0x87654321);}
    }
    /* Exact accessible ends: SIMD loads must never reach the guard page. */
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *pages=mmap(NULL,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(pages!=MAP_FAILED);assert(!mprotect(pages+page,page,PROT_NONE));
    for(unsigned size=2;size<=4;size+=2)for(unsigned n=1;n<65;n++) {
        unsigned char *p=pages+page-(size_t)n*size;
        for(unsigned i=0;i<n;i++)memcpy(p+(size_t)i*size,&i,size);
        unsigned lo=99,hi=99;assert(candidate(p,size,n,n,0,(size_t)n*size,&lo,&hi));assert(lo==0&&hi==n-1);
        /* Reject a short span before dereferencing inaccessible storage. */
        lo=hi=99;assert(!candidate(pages+page,size,n,n,0,(size_t)n*size-1,&lo,&hi));assert(lo==99&&hi==99);
    }
    munmap(pages,page*2);
    unsigned lo=0,hi=0;assert(!candidate(mem,3,10,20,0,30,&lo,&hi));
    for(unsigned i=0;i<300000;i++){unsigned v=i;memcpy(mem+i*4,&v,4);}
    assert(candidate(mem,4,300000,300000,0,1200000,&lo,&hi));assert(lo==0&&hi==299999);
    assert(accepted>5000&&declined>1000);
    printf("Index scan PASS24000 differential cases accepted=%u rejected=%u; unaligned,16/32bit,base overflow,short span,guard pages,300000 indices\n",accepted,declined);
    volatile unsigned checksum=0;double elapsed[2];const unsigned loops=100000,n=1024;
    for(unsigned i=0;i<n;i++){uint16_t v=(i*37)%1024;memcpy(mem+i*2,&v,2);}
    for(unsigned k=0;k<2;k++) {
        double start=seconds();for(unsigned i=0;i<loops;i++) {
            int ok=k?candidate(mem,2,n,n,0,n*2,&lo,&hi):reference(mem,2,n,n,0,&lo,&hi);
            assert(ok);checksum+=lo+hi;
        }elapsed[k]=seconds()-start;
    }
    printf("Index scan kernel %u x %u indices: scalar=%.6fs candidate=%.6fs ratio=%.3fx checksum=%u (not FPS)\n",loops,n,elapsed[0],elapsed[1],elapsed[0]/elapsed[1],checksum);
    free(mem);return 0;
}
