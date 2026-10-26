/* Native nonlocal transfer for checked MSVC _setjmp3/_longjmp callsites.
 * The original setjmp still writes its actual 32-bit guest environment.
 * Longjmp restores that environment then unwinds the native C frames to the
 * live translated caller. It never returns into the decoder's error path. */
#include "win32hle.h"
#include "nonlocal.h"
#include <stdio.h>
#include <string.h>
static _Thread_local DS_JmpFrame *head;
void ds_jmp_enter(DS_JmpFrame *f,uint32_t owner){
    memset(f,0,sizeof *f);f->owner=owner;f->active=1;f->previous=head;head=f;
}
void ds_jmp_leave(DS_JmpFrame *f){
    DS_JmpFrame **p=&head;while(*p&&*p!=f)p=&(*p)->previous;
    if(!*p)hle_fatal("nonlocal frame %#x is not live on this thread",f->owner);
    *p=f->previous;f->active=f->armed=0;
}
void ds_jmp_bind(DS_JmpFrame*f,uint32_t buffer,uint32_t caller_esp){
    if(!f->active||!buffer)hle_fatal("invalid guest setjmp frame/buffer %#x",buffer);
    f->buffer=buffer;f->caller_esp=caller_esp;f->armed=0;
}
void ds_jmp_arm(DS_JmpFrame*f){
    if(MEM32(f->buffer+0x20)!=0x56433230u||MEM32(f->buffer+0x10)+4u!=f->caller_esp)
        hle_fatal("guest setjmp environment layout mismatch at %#x",f->buffer);
    f->armed=1;
}
unsigned ds_jmp_depth(void){unsigned n=0;for(DS_JmpFrame*f=head;f;f=f->previous)++n;return n;}
_Noreturn void ds_guest_longjmp(void){
    uint32_t buffer=A32(0),value=A32(1);
    DS_JmpFrame*f=head;while(f&&(!f->active||!f->armed||f->buffer!=buffer))f=f->previous;
    if(!f)hle_fatal("guest longjmp target %#x has no live setjmp caller",buffer);
    if(MEM32(buffer+0x20)!=0x56433230u||MEM32(buffer+0x10)+4u!=f->caller_esp)
        hle_fatal("corrupted guest longjmp environment at %#x",buffer);
    uint32_t saved_fs=MEM32(buffer+0x18),unwind=MEM32(buffer+0x24);
    if(unwind)hle_fatal("guest longjmp %#x needs unsupported custom SEH unwind callback %#x",buffer,unwind);
    if(MEM32(g_fs_base)!=saved_fs)
        hle_fatal("guest longjmp %#x needs SEH-chain unwind: current=%#x saved=%#x",buffer,MEM32(g_fs_base),saved_fs);
    /* Discard metadata belonging to native frames this longjmp will unwind. */
    for(DS_JmpFrame*above=head;above&&above!=f;above=above->previous)above->active=above->armed=0;
    head=f;
    g_ebp=MEM32(buffer);g_ebx=MEM32(buffer+4);g_edi=MEM32(buffer+8);g_esi=MEM32(buffer+12);
    g_esp=MEM32(buffer+16)+4u;g_eax=value?value:1u;g_edx=buffer;MEM32(g_fs_base)=saved_fs;g_cur_func=f->owner;
    fprintf(stderr,"[guest-longjmp] env=%#x value=%#x caller=%#x\n",buffer,g_eax,f->owner);
    longjmp(f->environment,1);
}
