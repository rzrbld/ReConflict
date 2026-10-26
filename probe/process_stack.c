#define _GNU_SOURCE
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <elf.h>
#include <asm/ptrace.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
/* Diagnostic only: attach one requested game thread, read registers and its
 * frame-pointer chain, then detach. Never linked into the game. */
int main(int argc,char **argv){
 if(argc!=2)return 2;
 char *end;long number=strtol(argv[1],&end,10);if(*end||number<=1)return 2;
 pid_t pid=(pid_t)number;char path[80],name[80]={0};
 snprintf(path,sizeof path,"/proc/%ld/comm",number);FILE*f=fopen(path,"r");
 if(!f||!fgets(name,sizeof name,f)){if(f)fclose(f);return 2;}fclose(f);
 if(strncmp(name,"desert-storm",12)){fprintf(stderr,"Not the requested game process\n");return 2;}
 if(ptrace(PTRACE_ATTACH,pid,0,0)){perror("attach");return 1;}
 int status,rc=1;while(waitpid(pid,&status,0)<0){if(errno!=EINTR)goto detach;}
 if(!WIFSTOPPED(status))goto detach;
 struct user_pt_regs regs;struct iovec io={&regs,sizeof regs};
 if(ptrace(PTRACE_GETREGSET,pid,(void*)NT_PRSTATUS,&io)){perror("getregs");goto detach;}
 printf("pc=%#llx sp=%#llx fp=%#llx lr=%#llx\n",regs.pc,regs.sp,regs.regs[29],regs.regs[30]);
 uint64_t fp=regs.regs[29];
 for(unsigned i=0;i<40&&fp>=regs.sp&&fp-regs.sp<16*1024*1024;i++){
  uint64_t frame[2];struct iovec local={frame,sizeof frame},remote={(void*)(uintptr_t)fp,sizeof frame};
  if(process_vm_readv(pid,&local,1,&remote,1,0)!=sizeof frame)break;
  printf("frame%u fp=%#llx return=%#llx\n",i,(unsigned long long)fp,(unsigned long long)frame[1]);
  if(frame[0]<=fp)break;
  fp=frame[0];
 }
 rc=0;
detach:
 if(ptrace(PTRACE_DETACH,pid,0,0)){perror("detach");return 1;}return rc;
}
