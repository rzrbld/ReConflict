#include "cpu_features.h"
void ds_guest_cpuid(uint32_t leaf, uint32_t subleaf, uint32_t out[4]) {
    (void)subleaf;
    for (int i = 0; i < 4; ++i) out[i] = 0;
    switch (leaf) {
        case 0: /* GenuineIntel */
            out[0] = 1; out[1] = 0x756e6547u; out[2] = 0x6c65746eu; out[3] = 0x49656e69u; break;
        case 1:
            out[0] = 0x633u; out[3] = (1u<<0) | (1u<<4) | (1u<<15); break;
        case 0x80000000u: out[0] = 0x80000001u; break;
        default: break;
    }
}
