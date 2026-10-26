#ifndef DS_FP_STACK_INLINE_H
#define DS_FP_STACK_INLINE_H
#include <stdint.h>
#include <string.h>
#if defined(__aarch64__)
#include <arm_neon.h>
#endif
/* Preserve the existing exact helper's raw 8-slot window. The value barrier
 * retains rounding boundaries across former out-of-line calls; it deliberately
 * has no memory clobber, so independent guest register values can stay local.
 * This header is experimental and is injected only into explicitly staged code.
 */
static inline __attribute__((always_inline)) double ds_fp_value_barrier(double v) {
#if defined(__aarch64__)
    __asm__("" : "+w"(v));
#elif defined(__x86_64__)
    __asm__("" : "+x"(v));
#else
    volatile double rounded=v; v=rounded;
#endif
    return v;
}
static inline __attribute__((always_inline)) void ds_fp_push_inline(double st[8],int64_t shadow[8],int *top,double value) {
    value=ds_fp_value_barrier(value);
#if defined(__aarch64__)
    const uint8_t *s=(const uint8_t*)st,*h=(const uint8_t*)shadow;
    uint8x16_t a=vld1q_u8(s),b=vld1q_u8(s+16),c=vld1q_u8(s+32);
    uint8x8_t d=vld1_u8(s+48);
    uint8x16_t x=vld1q_u8(h),y=vld1q_u8(h+16),z=vld1q_u8(h+32);
    uint8x8_t w=vld1_u8(h+48);
    vst1q_u8((uint8_t*)st+8,a);vst1q_u8((uint8_t*)st+24,b);vst1q_u8((uint8_t*)st+40,c);vst1_u8((uint8_t*)st+56,d);
    vst1q_u8((uint8_t*)shadow+8,x);vst1q_u8((uint8_t*)shadow+24,y);vst1q_u8((uint8_t*)shadow+40,z);vst1_u8((uint8_t*)shadow+56,w);
#else
    uint64_t bits[7],old_shadow[7];
    memcpy(bits,st,56);memcpy(old_shadow,shadow,56);
    memcpy((uint8_t*)st+8,bits,56);memcpy((uint8_t*)shadow+8,old_shadow,56);
#endif
    memcpy(st,&value,8);shadow[0]=0;(*top)++;
}
static inline __attribute__((always_inline)) double ds_fp_pop_inline(double st[8],int64_t shadow[8],int *top) {
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
    uint64_t zero=0;memcpy(st+7,&zero,8);shadow[7]=0;(*top)--;
    return ds_fp_value_barrier(value);
}
#undef fp_push
#undef fp_pop
#define fp_push(v) ds_fp_push_inline(_st,g_st_i64,&_fp_top,(v))
#define fp_pop() ds_fp_pop_inline(_st,g_st_i64,&_fp_top)
#endif
