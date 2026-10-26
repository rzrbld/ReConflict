#include "fp_stack_exact.h"
#include <string.h>
#if defined(__aarch64__)
#include <arm_neon.h>
#endif
/* Byte-vector operations deliberately avoid arithmetic on float/NaN values.
 * Load all source lanes before overwriting their overlapping destinations.
 * No always_inline annotation or per-generated-TU copy is required. */
void ds_fp_stack_push_exact(double st[8],int64_t shadow[8],int *top,double value){
#if defined(__aarch64__)
    const uint8_t *s=(const uint8_t*)st,*h=(const uint8_t*)shadow;
    uint8x16_t a=vld1q_u8(s),b=vld1q_u8(s+16),c=vld1q_u8(s+32);
    uint8x8_t d=vld1_u8(s+48);
    uint8x16_t x=vld1q_u8(h),y=vld1q_u8(h+16),z=vld1q_u8(h+32);
    uint8x8_t w=vld1_u8(h+48);
    vst1q_u8((uint8_t*)st+8,a);vst1q_u8((uint8_t*)st+24,b);vst1q_u8((uint8_t*)st+40,c);vst1_u8((uint8_t*)st+56,d);
    vst1q_u8((uint8_t*)shadow+8,x);vst1q_u8((uint8_t*)shadow+24,y);vst1q_u8((uint8_t*)shadow+40,z);vst1_u8((uint8_t*)shadow+56,w);
#else
    /* Portable semantic fallback for host verification. Real AArch64 uses the
     * explicit NEON path above, regardless of compiler loop recognition. */
    uint64_t bits[7],old_shadow[7];
    memcpy(bits,st,56);memcpy(old_shadow,shadow,56);
    memcpy((uint8_t*)st+8,bits,56);memcpy((uint8_t*)shadow+8,old_shadow,56);
#endif
    memcpy(st,&value,8);shadow[0]=0;(*top)++;
}
double ds_fp_stack_pop_exact(double st[8],int64_t shadow[8],int *top){
    double value;memcpy(&value,st,8);
#if defined(__aarch64__)
    const uint8_t *s=(const uint8_t*)st,*h=(const uint8_t*)shadow;
    uint8x16_t a=vld1q_u8(s+8),b=vld1q_u8(s+24),c=vld1q_u8(s+40);
    uint8x8_t d=vld1_u8(s+56);
    uint8x16_t x=vld1q_u8(h+8),y=vld1q_u8(h+24),z=vld1q_u8(h+40);
    uint8x8_t w=vld1_u8(h+56);
    vst1q_u8((uint8_t*)st,a);vst1q_u8((uint8_t*)st+16,b);vst1q_u8((uint8_t*)st+32,c);vst1_u8((uint8_t*)st+48,d);
    vst1q_u8((uint8_t*)shadow,x);vst1q_u8((uint8_t*)shadow+16,y);vst1q_u8((uint8_t*)shadow+32,z);vst1_u8((uint8_t*)shadow+48,w);
#else
    uint64_t bits[7],old_shadow[7];
    memcpy(bits,(const uint8_t*)st+8,56);memcpy(old_shadow,(const uint8_t*)shadow+8,56);
    memcpy(st,bits,56);memcpy(shadow,old_shadow,56);
#endif
    uint64_t zero=0;memcpy(st+7,&zero,8);shadow[7]=0;(*top)--;return value;
}
