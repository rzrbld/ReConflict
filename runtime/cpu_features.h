#ifndef DS_CPU_FEATURES_H
#define DS_CPU_FEATURES_H
#include <stdint.h>
/* A conservative virtual x86 CPU: x87/TSC/CMOV, no SIMD or 3DNow advertised.
 * This is a proposed dispatch policy; fallback-path reachability is untested. */
void ds_guest_cpuid(uint32_t leaf, uint32_t subleaf, uint32_t out[4]);
#undef CPUID
#define CPUID(a,b,c,d) do { \
    uint32_t ds_leaf_ = (a), ds_sub_ = (c), ds_result_[4]; \
    RECOMP_REGS_OUT(); ds_guest_cpuid(ds_leaf_, ds_sub_, ds_result_); \
    g_eax = ds_result_[0]; g_ebx = ds_result_[1]; \
    g_ecx = ds_result_[2]; g_edx = ds_result_[3]; \
    RECOMP_REGS_IN(); \
} while (0)
#endif
