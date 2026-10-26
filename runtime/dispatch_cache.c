#include "dispatch_cache.h"
#include <pthread.h>
#ifdef __FAST_MATH__
#error Dispatch cache must preserve ordinary control-flow semantics
#endif
_Static_assert(DS_DISPATCH_CACHE_SIZE>0 &&
    (DS_DISPATCH_CACHE_SIZE&(DS_DISPATCH_CACHE_SIZE-1))==0,
    "dispatch cache size must be a power of two");
/* Only immutable main executable positives enter these TLS slots. Dynamic
 * recovered/modules/imports are resolved before this helper is reached. */
static __thread struct { uint32_t address; recomp_func_t func; } slots[DS_DISPATCH_CACHE_SIZE];
static __thread unsigned long hits,searches,unknowns;
int ds_dispatch_cache_active;
static int stats_active;
static pthread_once_t report_once=PTHREAD_ONCE_INIT;
void ds_dispatch_cache_report(void){
    if(!stats_active)return;
    fprintf(stderr,"[dispatch-cache] current-thread enabled=%d size=%u hits=%lu searches=%lu unknowns=%lu\n",
        ds_dispatch_cache_active,(unsigned)DS_DISPATCH_CACHE_SIZE,hits,searches,unknowns);
}
static void register_report(void){atexit(ds_dispatch_cache_report);}
void ds_dispatch_cache_set_stats(int enabled){
    stats_active=!!enabled;
    if(stats_active)pthread_once(&report_once,register_report);
}
void ds_dispatch_cache_set_enabled(int enabled){ds_dispatch_cache_active=!!enabled;}
static void __attribute__((constructor)) initialize(void){
    const char *s=getenv("DS_DISPATCH_CACHE");
    ds_dispatch_cache_set_enabled(s&&!strcmp(s,"1"));
    s=getenv("DS_DISPATCH_CACHE_STATS");
    if(ds_dispatch_cache_active&&s&&!strcmp(s,"1"))ds_dispatch_cache_set_stats(1);
}
#ifdef DS_LOOKUP_CACHE_TEST
static __thread unsigned long test_hits,test_searches;
void ds_lookup_test_reset(void){memset(slots,0,sizeof slots);hits=searches=unknowns=test_hits=test_searches=0;}
unsigned long ds_lookup_test_hits(void){return test_hits;}
unsigned long ds_lookup_test_searches(void){return test_searches;}
#endif
recomp_func_t ds_dispatch_main_lookup(uint32_t va){
    unsigned index=((va>>4)^(va>>12)^va)&(DS_DISPATCH_CACHE_SIZE-1);
    if(slots[index].func&&slots[index].address==va){
        if(stats_active)++hits;
#ifdef DS_LOOKUP_CACHE_TEST
        ++test_hits;
#endif
        return slots[index].func;
    }
    if(stats_active)++searches;
#ifdef DS_LOOKUP_CACHE_TEST
    ++test_searches;
#endif
    uint32_t lo=0,hi=recomp_dispatch_count;
    while(lo<hi){
        uint32_t mid=lo+(hi-lo)/2,m=recomp_dispatch_table[mid].address;
        if(m==va){
            recomp_func_t fn=recomp_dispatch_table[mid].func;
            if(fn){slots[index].address=va;slots[index].func=fn;}
            return fn;
        }
        if(m<va)lo=mid+1;else hi=mid;
    }
    if(stats_active)++unknowns;
    return NULL;
}
