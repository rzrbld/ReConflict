#include "win32hle.h"
#include "dispatch_cache.h"
#include <assert.h>
#include <time.h>
static void guest(void){}
#include "benchmark-table.inc"
recomp_func_t ds_recovered_lookup(uint32_t va){(void)va;return NULL;}
recomp_func_t ds_module_lookup(uint32_t va){(void)va;return NULL;}
extern void ds_lookup_test_reset(void);
extern unsigned long ds_lookup_test_hits(void),ds_lookup_test_searches(void);
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(void){
 assert(!recomp_lookup(0));assert(!recomp_lookup(0));
 const unsigned sets[]={1,16,64,256,1024,12235};
 for(unsigned k=0;k<sizeof sets/sizeof *sets;k++){
  unsigned set=sets[k],indices[12235];for(unsigned i=0;i<set;i++)indices[i]=(i*977u)%recomp_dispatch_count;
  ds_lookup_test_reset();const unsigned iterations=2000000;volatile recomp_func_t sink=NULL;double t=seconds();for(unsigned i=0;i<iterations;i++)sink=recomp_lookup(recomp_dispatch_table[indices[i%set]].address);double elapsed=seconds()-t;assert(sink);
  printf("cache_compiled=%d runtime_gate=%d synthetic_uniform_set=%u actual_table=%u ns=%.3f hitpct=%.2f\n",DS_MAIN_LOOKUP_CACHE,ds_dispatch_cache_active,set,recomp_dispatch_count,elapsed*1e9/iterations,100.0*ds_lookup_test_hits()/iterations);
 }
 return 0;
}
