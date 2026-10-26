#ifndef DS_PRESENT_TIMING_H
#define DS_PRESENT_TIMING_H
#include <stdint.h>
typedef struct {
    int enabled;unsigned frame;
    double guest_tail_ms;
    uint64_t wall_begin,cpu_begin,wall_input,cpu_input,wall_overlay,cpu_overlay;
    uint64_t worker_before,worker_after;
} DsPresentTiming;
uint64_t ds_present_cpu_ns(void);
DsPresentTiming ds_present_timing_begin(unsigned frame,double guest_tail_ms);
void ds_present_timing_input(DsPresentTiming *t);
void ds_present_timing_overlay(DsPresentTiming *t);
void ds_present_timing_end(DsPresentTiming *t);
void ds_present_timing_flush(void);
#endif
