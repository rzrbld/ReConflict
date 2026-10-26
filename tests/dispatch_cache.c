#include "win32hle.h"
#include "dispatch_cache.h"
#include <assert.h>
#include <time.h>
#include <pthread.h>
static unsigned guards,main_calls,shim_a,shim_b,recovered_checks,module_checks;
static unsigned active_module,recovered_override;
static void guest_a(void){main_calls++;}
static void guest_b(void){main_calls+=10;}
static void recovery(void){}
static void module_a(void){}
static void module_b(void){}
static void import_a(void){shim_a++;assert(!strcmp(g_cur_import,"first"));}
static void import_b(void){shim_b++;assert(!strcmp(g_cur_import,"second"));}
const recomp_dispatch_entry_t recomp_dispatch_table[]={{0,guest_b},{0x401000,guest_a},{0x401444,guest_b},{0x401888,guest_a},{0x401ccc,guest_b},{0x401eee,NULL}};
const uint32_t recomp_dispatch_count=sizeof recomp_dispatch_table/sizeof *recomp_dispatch_table;
recomp_func_t ds_recovered_lookup(uint32_t va){recovered_checks++;return recovered_override&&va==0x401000?recovery:NULL;}
recomp_func_t ds_module_lookup(uint32_t va){module_checks++;return active_module&&(va==0x401000||va==0x10001000)?(active_module==1?module_a:module_b):NULL;}
void ds_guard_dispatch(uint32_t va){(void)va;guards++;}
extern void ds_lookup_test_reset(void);
extern unsigned long ds_lookup_test_hits(void),ds_lookup_test_searches(void);
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static void*thread_test(void*p){(void)p;assert(ds_lookup_test_hits()==0);assert(ds_lookup_test_searches()==0);assert(recomp_lookup(0x401000)==guest_a);assert(ds_lookup_test_searches()==(unsigned long)ds_dispatch_cache_active);return NULL;}
int main(void){
 int enabled=DS_MAIN_LOOKUP_CACHE && getenv("DS_DISPATCH_CACHE")&&!strcmp(getenv("DS_DISPATCH_CACHE"),"1");
 ds_lookup_test_reset();
 assert(recomp_lookup(0)==guest_b);assert(recomp_lookup(0)==guest_b);
 assert(recomp_lookup(0x401000)==guest_a);assert(recomp_lookup(0x401000)==guest_a);
 assert(recovered_checks==4&&module_checks==4);
if(enabled)assert(ds_lookup_test_hits()==2&&ds_lookup_test_searches()==2);else assert(ds_lookup_test_hits()==0&&ds_lookup_test_searches()==0);
 /* Positive main cache must not mask later recovered or module overrides. */
 recovered_override=1;active_module=1;assert(recomp_lookup(0x401000)==recovery);recovered_override=0;assert(recomp_lookup(0x401000)==module_a);
 active_module=2;assert(recomp_lookup(0x401000)==module_b);active_module=0;assert(recomp_lookup(0x401000)==guest_a);
 assert(!recomp_lookup(0x10001000));active_module=1;assert(recomp_lookup(0x10001000)==module_a);active_module=0;assert(!recomp_lookup(0x10001000));active_module=2;assert(recomp_lookup(0x10001000)==module_b);active_module=0;
 unsigned long searches=ds_lookup_test_searches();assert(!recomp_lookup(0xdeadbeef));assert(!recomp_lookup(0xdeadbeef));assert(ds_lookup_test_searches()==searches+(enabled?2u:0u));
 searches=ds_lookup_test_searches();assert(!recomp_lookup(0x401eee));assert(!recomp_lookup(0x401eee));assert(ds_lookup_test_searches()==searches+(enabled?2u:0u));
 /* Deliberate same-slot positives exercise safe replacement, never stale fn. */
 for(unsigned i=0;i<100;i++){assert(recomp_lookup(0x401000)==guest_a);assert(recomp_lookup(0x401444)==guest_b);assert(recomp_lookup(0x401888)==guest_a);assert(recomp_lookup(0x401ccc)==guest_b);}
 win32hle_shim shims[]={{"first",import_a},{"second",import_b},{NULL,NULL}};assert(win32hle_register(shims)==2);uint32_t a=hle_resolve("first"),b=hle_resolve("second");
 for(unsigned i=0;i<10;i++){recomp_func_t fn=recomp_lookup_manual(a);assert(fn);fn();fn=recomp_lookup_manual(b);assert(fn);fn();}
 assert(shim_a==10&&shim_b==10&&!guards);assert(!recomp_lookup_manual(a+1)&&guards==1);
 /* No unknown/NULL shim result may poison later registration. */
 uint32_t next=b+16;assert(!recomp_lookup_manual(next)&&guards==2);win32hle_shim later[]={{"later",import_a},{NULL,NULL}};assert(win32hle_register(later)==1);assert(recomp_lookup_manual(next));
 ds_lookup_test_reset();assert(recomp_lookup(0x401000)==guest_a);pthread_t thread;assert(!pthread_create(&thread,NULL,thread_test,NULL));assert(!pthread_join(thread,NULL));assert(ds_lookup_test_searches()==(unsigned long)enabled);
 /* Live toggles do not require invalidating immutable-main entries. */
 ds_dispatch_cache_set_enabled(1);assert(recomp_lookup(0x401000)==guest_a);
 ds_dispatch_cache_set_enabled(0);assert(recomp_lookup(0x401000)==guest_a);recovered_override=1;assert(recomp_lookup(0x401000)==recovery);recovered_override=0;
 ds_dispatch_cache_set_enabled(1);active_module=1;assert(recomp_lookup(0x401000)==module_a);active_module=0;assert(recomp_lookup(0x401000)==guest_a);
 ds_dispatch_cache_set_stats(1);assert(recomp_lookup(0x401000)==guest_a);ds_dispatch_cache_report();ds_dispatch_cache_set_stats(0);
 ds_dispatch_cache_set_enabled(enabled);
 puts("PASS main cache: precedence, module load/unload/reload/overlap, collisions, misses, VA0, shim side effects, late registration, TLS lifetime");
 const unsigned iterations=3000000;volatile recomp_func_t sink=NULL;double t=seconds();for(unsigned i=0;i<iterations;i++)sink=recomp_lookup(i%4==0?0x401000:0x401444);double elapsed=seconds()-t;assert(sink);printf("cache_compiled=%d runtime_gate=%d host small-table lookup ns=%.3f (%u lookups), hits=%lu searches=%lu\n",DS_MAIN_LOOKUP_CACHE,enabled,elapsed*1e9/iterations,iterations,ds_lookup_test_hits(),ds_lookup_test_searches());return 0;
}
