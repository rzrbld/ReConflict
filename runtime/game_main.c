#define _GNU_SOURCE
#include "win32hle.h"
#include "host.h"
#include "paths.h"
#include "profiler.h"
#include "configuration.h"
#include <SDL.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <execinfo.h>

extern const win32hle_shim ds_d3d8_shims[] __attribute__((weak));
extern const win32hle_shim ds_dinput8_shims[] __attribute__((weak));
extern const win32hle_shim ds_dsound8_shims[] __attribute__((weak));
extern const win32hle_shim ds_win32_extra_shims[] __attribute__((weak));
extern void ds_seed_game_settings(void) __attribute__((weak));
extern void ds_startup_dialog_hook(uint32_t,uint32_t) __attribute__((weak));
static void game_exit(int code){(void)code;ds_profiler_stop();}
static volatile sig_atomic_t stop_requested,snapshot_requested;
extern void ds_graphics_profile_flush(void) __attribute__((weak));
static void runtime_signal(int sig){if(sig==SIGUSR1)snapshot_requested=1;else stop_requested=sig;}
/* Called at ordinary input-pump boundaries, never from the signal handler. */
void ds_runtime_poll(void){
    if(snapshot_requested){snapshot_requested=0;if(ds_graphics_profile_flush)ds_graphics_profile_flush();ds_profiler_stop();ds_profiler_start();}
    if(stop_requested){fprintf(stderr,"[host] requested stop signal=%d\n",stop_requested);hle_exit(0);}
}

void ds_guard_dispatch(uint32_t va) {
    if(va==RECOMP_RETADDR)return;
    if(recomp_lookup(va))return;
    /* hle_name checks existence without altering the pending shim index. */
    if(hle_name(va))return;
    hle_fatal("unresolved guest dispatch target %#x from %#x",va,g_cur_func);
}

static void crashed(int sig,siginfo_t *info,void *context) {
    (void)context;
    ds_profiler_stop();
    dprintf(2,"\nGAME FAULT signal=%d address=%p guest_base=%p current_guest=%#x import=%s\n"
            "eax=%#x ebx=%#x ecx=%#x edx=%#x esp=%#x ebp=%#x esi=%#x edi=%#x fs=%#x\n",
            sig,info->si_addr,(void*)g_mem_base,g_cur_func,g_cur_import?g_cur_import:"(none)",
            g_eax,g_ebx,g_ecx,g_edx,g_esp,g_ebp,g_esi,g_edi,g_fs_base);
    void *trace[32];int n=backtrace(trace,32);backtrace_symbols_fd(trace,n,2);
    _exit(128+sig);
}
int main(int argc,char **argv) {
    setvbuf(stderr,NULL,_IONBF,0);setvbuf(stdout,NULL,_IOLBF,0);
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS,"1");
    struct sigaction sa;memset(&sa,0,sizeof sa);sa.sa_sigaction=crashed;sa.sa_flags=SA_SIGINFO;
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);sigaction(SIGFPE,&sa,NULL);sigaction(SIGBUS,&sa,NULL);
    struct sigaction control;memset(&control,0,sizeof control);control.sa_handler=runtime_signal;control.sa_flags=SA_RESTART;
    sigaction(SIGTERM,&control,NULL);sigaction(SIGINT,&control,NULL);sigaction(SIGUSR1,&control,NULL);
    if(ds_paths_init()){perror("game paths");return 1;}
    if(argc>1 && !strcmp(argv[1],"--configure"))return ds_configuration_main();
    const char *root=getenv("DS_ROOT");char data[PATH_MAX],reg[PATH_MAX];
    snprintf(data,sizeof data,"%s/data",root);snprintf(reg,sizeof reg,"%s/config/registry.txt",root);
    if(chdir(data)){perror(data);return 1;}
    hle_set_drive('C',data);hle_set_command_line("C:\\DesertStorm.exe");
    hle_set_drive('S',getenv("DS_SAVE_DIR"));
    char userdata[PATH_MAX];snprintf(userdata,sizeof userdata,"%s/userdata",root);
    hle_set_overlay(data,userdata);
    if(ds_d3d8_shims)win32hle_register(ds_d3d8_shims);
    if(ds_dinput8_shims)win32hle_register(ds_dinput8_shims);
    if(ds_dsound8_shims)win32hle_register(ds_dsound8_shims);
    if(ds_win32_extra_shims)win32hle_register(ds_win32_extra_shims);
    recomp_host_init();
    hle_reg_load(reg);
    if(ds_seed_game_settings)ds_seed_game_settings();
    if(ds_startup_dialog_hook)hle_dialog_hook=ds_startup_dialog_hook;
    hle_on_exit=game_exit;
    win32hle_trace=getenv("DS_TRACE_IMPORTS")!=NULL;
    fprintf(stderr,"Starting statically recompiled Desert Storm (game entry, not diagnostic)\n");
    int permissive=argc>1 && !strcmp(argv[1],"--permissive");
    int rc=permissive?recomp_host_boot_permissive("DesertStorm.exe"):recomp_host_boot("DesertStorm.exe");
    if(rc)return rc;
    const char *save_literal="C:\\Documents and Settings\\%s\\My Documents\\SCi\\DesertStorm";
    char *guest_save=(char*)hle_ptr(0x5e6bfcu);
    if(strcmp(guest_save,save_literal))hle_fatal("loaded game save-path anchor mismatch");
    memset(guest_save,0,strlen(save_literal)+1);memcpy(guest_save,"S:\\",4);
    fprintf(stderr,"[paths] original save root redirected to %s beside executable\n",getenv("DS_SAVE_DIR"));
    fprintf(stderr,"Calling original PE entry %#x\n",recomp_host_entry());
    ds_profiler_start();
    return (int)recomp_host_run();
}
