/* Read-only running-game input sampler. Never ptrace-stops or writes process
 * memory. Native symbol offsets must match the exact deployed executable. */
#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int memory_fd;
static uint64_t guest_base;
static uint64_t number(const char *s){char *end;errno=0;unsigned long long v=strtoull(s,&end,0);if(errno||!*s||*end||*s=='-'){fprintf(stderr,"Invalid unsigned number: %s\n",s);exit(2);}return v;}
static int native_read(uint64_t address,void *out,size_t bytes){
    if(address>INT64_MAX||bytes>(uint64_t)INT64_MAX-address)return -1;
    ssize_t n=pread(memory_fd,out,bytes,(off_t)address);return n==(ssize_t)bytes?0:-1;
}
static uint32_t guest32(uint32_t va,int *bad){uint32_t out=0;if(native_read(guest_base+va,&out,sizeof out))*bad=1;return out;}
static uint16_t guest16(uint32_t va,int *bad){uint16_t out=0;if(native_read(guest_base+va,&out,sizeof out))*bad=1;return out;}
static uint64_t nanos(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
int main(int argc,char **argv){
    if(argc<4||argc>6){fprintf(stderr,"Usage: %s PID PIE_LOAD_BIAS G_MEM_BASE_SYMBOL_OFFSET [HZ<=100 [SECONDS<=5]]\n",argv[0]);return 2;}
    uint64_t pid=number(argv[1]),bias=number(argv[2]),symbol=number(argv[3]),hz=argc>4?number(argv[4]):100,seconds=argc>5?number(argv[5]):5;
    if(!pid||pid>INT32_MAX||!hz||hz>100||!seconds||seconds>5||symbol>UINT64_MAX-bias){fprintf(stderr,"Require valid PID, 1..100 Hz and 1..5 seconds\n");return 2;}
    char path[64];snprintf(path,sizeof path,"/proc/%"PRIu64"/mem",pid);memory_fd=open(path,O_RDONLY|O_CLOEXEC);
    if(memory_fd<0){perror(path);return 1;}
    if(native_read(bias+symbol,&guest_base,sizeof guest_base)||guest_base>INT64_MAX-UINT32_MAX){fprintf(stderr,"Cannot read a valid guest base at native %#"PRIx64"\n",bias+symbol);close(memory_fd);return 1;}
    fprintf(stderr,"Read-only sampler: pid=%"PRIu64" PIE=%#"PRIx64" g_mem_base symbol=%#"PRIx64" guest_base=%#"PRIx64" hz=%"PRIu64" seconds=%"PRIu64"\n",pid,bias,symbol,guest_base,hz,seconds);
    puts("elapsed_ms,logical_a,logical_up,menu_forward,menu_back,menu_up,menu_down,device_mask,joystick_index,joystick_count,frontend,players,game_clock_ms,repeat_deadline_ms,main_menu_pointer,main_selection");
    uint64_t start=nanos(),period=1000000000u/hz;
    for(uint64_t i=0;i<hz*seconds&&nanos()-start<seconds*1000000000u;++i){
        uint64_t sample_started=nanos();
        int bad=0;uint32_t la=guest32(0x7541c0,&bad),lu=guest32(0x7541e8,&bad);
        uint32_t forward=guest32(0x60f744,&bad),back=guest32(0x60f740,&bad),up=guest32(0x60f730,&bad),down=guest32(0x60f734,&bad);
        uint32_t mask=guest32(0x60f5f0,&bad),joy=guest32(0x60f980,&bad),count=guest32(0x754be8,&bad),front=guest32(0x617c18,&bad),players=guest16(0x610798,&bad);
        uint32_t clock_pointer=guest32(0x63c958,&bad),clock=0,deadline=guest32(0x60fa14,&bad),menu=guest32(0x617b94,&bad),selection=UINT16_MAX;
        if(clock_pointer&&clock_pointer<UINT32_MAX-0x14)clock=guest32(clock_pointer+0x10,&bad);
        if(menu&&menu<UINT32_MAX-0x2a)selection=guest16(menu+0x28,&bad);
        if(bad){fprintf(stderr,"Process memory became unreadable at sample %"PRIu64" (process may have exited)\n",i);close(memory_fd);return 1;}
        printf("%.3f,%#x,%#x,%#x,%#x,%#x,%#x,%#x,%d,%u,%#x,%u,%u,%u,%#x,%u\n",(nanos()-start)/1e6,la,lu,forward,back,up,down,mask,(int32_t)joy,count,front,players,clock,deadline,menu,selection);
        uint64_t next=sample_started+period;struct timespec until={(time_t)(next/1000000000u),(long)(next%1000000000u)};
        while(clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&until,NULL)==EINTR){}
    }
    close(memory_fd);return 0;
}
