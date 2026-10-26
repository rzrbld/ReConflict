/* Exercise live translated-caller setjmp and real nested native unwinding.
 * Uses the original lifted _setjmp3 body, not a synthetic environment layout. */
#define RECOMP_GENERATED_CODE
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include "win32hle.h"
#include "nonlocal.h"
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_fs_base,g_gs_base,g_cur_func;
ptrdiff_t g_mem_base;
extern void recomp_desertstorm_005C9654(void);
static int unexpected_return;
void hle_fatal(const char*fmt,...){va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);fputc('\n',stderr);abort();}
static void *check_tls(void *unused){(void)unused;assert(ds_jmp_depth()==0);return NULL;}
static _Noreturn void deeper_error(uint32_t buffer,uint32_t value){
    g_ebp=0xdead0001;g_ebx=0xdead0002;g_edi=0xdead0003;g_esi=0xdead0004;g_esp-=64;
    PUSH32(g_esp,value);PUSH32(g_esp,buffer);PUSH32(g_esp,RECOMP_RETADDR);ds_guest_longjmp();
}
#define FLAGS uint32_t _flag_a=0,_flag_b=0,_flag_k=FK_NONE,_cf=0
static void nested_owner(uint32_t outer,uint32_t inner,uint32_t value){
    FLAGS;DS_JmpFrame frame;ds_jmp_enter(&frame,0x2222);
    PUSH32(g_esp,0);PUSH32(g_esp,inner);
    DS_GUEST_SETJMP(frame,recomp_desertstorm_005C9654);g_esp+=8;
    assert(g_eax==0);assert(ds_jmp_depth()==2);deeper_error(outer,value);
}
static void owner(uint32_t buffer,uint32_t value,int nested,int jump){
    FLAGS;DS_JmpFrame frame;ds_jmp_enter(&frame,0x1111);
    uint32_t original_esp=g_esp;
    g_ebp=0x01234567;g_ebx=0x12345678;g_edi=0x23456789;g_esi=0x3456789a;
    PUSH32(g_esp,0);PUSH32(g_esp,buffer);
    DS_GUEST_SETJMP(frame,recomp_desertstorm_005C9654);g_esp+=8;
    if(g_eax==0&&jump){
        pthread_t thread;assert(!pthread_create(&thread,NULL,check_tls,NULL));assert(!pthread_join(thread,NULL));
        if(nested)nested_owner(buffer,buffer+0x100,value);else deeper_error(buffer,value);
        unexpected_return=1;
    }
    if(jump){assert(g_eax==(value?value:1));assert(g_ebp==0x01234567);assert(g_ebx==0x12345678);assert(g_edi==0x23456789);assert(g_esi==0x3456789a);assert(g_esp==original_esp);assert(g_cur_func==0x1111);assert(ds_jmp_depth()==1);}
    ds_jmp_leave(&frame);
}
int main(void){
    g_mem_base=(ptrdiff_t)calloc(1,0x10000);assert(g_mem_base);g_fs_base=0x100;MEM32(g_fs_base)=UINT32_MAX;g_esp=0xf000;
    const uint32_t values[]={0,1,7,UINT32_MAX,0x80004005};
    for(unsigned i=0;i<sizeof values/sizeof *values;++i)for(int nested=0;nested<2;++nested){owner(0x1000,values[i],nested,1);assert(!unexpected_return);assert(ds_jmp_depth()==0);}
    owner(0x1000,1,0,0);assert(ds_jmp_depth()==0);
    pid_t child=fork();assert(child>=0);if(!child)deeper_error(0x1000,1);int status;assert(waitpid(child,&status,0)==child);assert(WIFSIGNALED(status)&&WTERMSIG(status)==SIGABRT);
    free((void*)g_mem_base);puts("Guest nonlocal jump: zero/nonzero values, nested native unwinding, saved registers/stack/FS, TLS and expired-context rejection PASS");return 0;
}
