#ifndef DS_INDEX_SCAN_H
#define DS_INDEX_SCAN_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#if defined(__aarch64__)
#include <arm_neon.h>
#endif
/* Returns1 only for a completely valid XYZ index range. On failure leave
 * outputs unchanged and retain the original scalar loop's first-error report.
 * Indexed fast scanning requires a facade-owned, bounded CPU index span.
 * RHW vertex checks remain in the caller's original loop. No persistent cache. */
static inline int ds_index_scan(const void *indices,unsigned size,unsigned n,
                              unsigned vcount,unsigned base,size_t span,
                              unsigned *minimum,unsigned *maximum) {
    if(!n)return 0;
    if(!indices) {
        if(n>vcount || n-1>UINT32_MAX-base)return 0;
        *minimum=0;*maximum=n-1;return 1;
    }
    if((size!=2 && size!=4) || n>span/size)return 0;
    unsigned lo=UINT32_MAX,hi=0,i=0;
    const unsigned char *p=indices;
#if defined(__aarch64__)
    if(size==2) {
        uint16x8_t mn=vdupq_n_u16(UINT16_MAX),mx=vdupq_n_u16(0);
        for(;n-i>=8;i+=8) {
            uint16x8_t v=vld1q_u16((const uint16_t*)(p+(size_t)i*2));
            mn=vminq_u16(mn,v);mx=vmaxq_u16(mx,v);
        }
        lo=vminvq_u16(mn);hi=vmaxvq_u16(mx);
    } else {
        uint32x4_t mn=vdupq_n_u32(UINT32_MAX),mx=vdupq_n_u32(0);
        for(;n-i>=4;i+=4) {
            uint32x4_t v=vld1q_u32((const uint32_t*)(p+(size_t)i*4));
            mn=vminq_u32(mn,v);mx=vmaxq_u32(mx,v);
        }
        lo=vminvq_u32(mn);hi=vmaxvq_u32(mx);
    }
#endif
    for(;i<n;i++) {
        unsigned ix;
        if(size==2) { uint16_t v;memcpy(&v,p+(size_t)i*2,2);ix=v; }
        else memcpy(&ix,p+(size_t)i*4,4);
        if(ix<lo)lo=ix;if(ix>hi)hi=ix;
    }
    if(hi>=vcount || hi>UINT32_MAX-base)return 0;
    *minimum=lo;*maximum=hi;return 1;
}
#endif
