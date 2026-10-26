#ifndef DS_FP_STACK_EXACT_H
#define DS_FP_STACK_EXACT_H
#include <stdint.h>
/* Out-of-line fixed-window x87 moves, with unchanged original representation.
 * Real runtime st/shadow/top are distinct global objects. Preserve all bits,
 * including inactive stack slots, signed zeros and signaling NaN payloads.
 * Caller must avoid signed top overflow, as in the original helpers. */
void ds_fp_stack_push_exact(double st[8],int64_t shadow[8],int *top,double value);
double ds_fp_stack_pop_exact(double st[8],int64_t shadow[8],int *top);
#endif
