#define _POSIX_C_SOURCE 200809L
#include "frame_limiter.h"
#include <assert.h>
int main(int argc,char**argv){
    assert(argc==2);int cap=atoi(argv[1]);assert(cap==0||cap==30||cap==60);
    setenv("DS_FRAME_CAP",argv[1],1);assert(ds_frame_cap_value()==cap);
    assert(ds_frame_swap_interval(1)==(cap?1:0));
    ds_frame_limit_present();uint64_t began=ds_frame_clock_ns();
    int count=cap?cap*2:120;
    for(int i=0;i<count;i++)ds_frame_limit_present();
    double seconds=(ds_frame_clock_ns()-began)/1e9;
    if(cap){assert(seconds>=1.99&&seconds<2.25);printf("cap=%d observed=%.3f fps\n",cap,count/seconds);}
    else {assert(seconds<0.5);printf("uncapped: no intentional wait (%.6f seconds)\n",seconds);}
    if(cap==30){
        struct timespec slow={0,45000000};nanosleep(&slow,NULL);
        began=ds_frame_clock_ns();ds_frame_limit_present();
        assert((ds_frame_clock_ns()-began)<10000000); /* No extra wait for a slow frame. */
    }
    return 0;
}
