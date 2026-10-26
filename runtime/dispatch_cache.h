#ifndef DS_DISPATCH_CACHE_H
#define DS_DISPATCH_CACHE_H
#include "recomp_types.h"
#ifndef DS_DISPATCH_CACHE_SIZE
#define DS_DISPATCH_CACHE_SIZE 1024u
#endif
/* Launch defaults: disabled; optional DS_DISPATCH_CACHE=1 and
 * DS_DISPATCH_CACHE_STATS=1. Live setters require guest-machine ownership.
 * The flag check occurs after recovered/live-module checks in host_lite.c. */
extern int ds_dispatch_cache_active;
void ds_dispatch_cache_set_enabled(int enabled);
void ds_dispatch_cache_set_stats(int enabled);
/* Cumulative CURRENT-thread counters, explicitly labelled; never aggregate. */
void ds_dispatch_cache_report(void);
recomp_func_t ds_dispatch_main_lookup(uint32_t va);
#ifdef DS_LOOKUP_CACHE_TEST
void ds_lookup_test_reset(void);
unsigned long ds_lookup_test_hits(void);
unsigned long ds_lookup_test_searches(void);
#endif
#endif
