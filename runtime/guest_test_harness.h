/* Minimal guest-memory/shim harness for isolated COM ABI tests. */
#ifndef DS_GUEST_TEST_HARNESS_H
#define DS_GUEST_TEST_HARNESS_H
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "win32hle.h"
uint32_t g_eax, g_esp;
ptrdiff_t g_mem_base;
static uint32_t next_alloc=0x10000;
static win32hle_shim methods[512];
static unsigned method_count;
static unsigned enumerated;
static uint8_t instance_guid[16];
void *hle_alloc(uint32_t n){uint32_t p=next_alloc;next_alloc+=(n+15)&~15u;assert(next_alloc<0x300000);memset(hle_ptr(p),0,n);return hle_ptr(p);}
void hle_free(void*p){(void)p;}
uint32_t hle_guest_strdup(const char*s){size_t n=strlen(s)+1;void*p=hle_alloc((uint32_t)n);memcpy(p,s,n);return hle_va(p);}
void hle_fatal(const char*fmt,...){va_list ap;va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);abort();}
uint32_t hle_va_bad(const void*p){(void)p;abort();}
int win32hle_register(const win32hle_shim*s){int n=0;while(s->name){assert(method_count<512);methods[method_count++]=*s++;++n;}return n;}
uint32_t hle_resolve(const char*n){for(unsigned i=0;i<method_count;++i)if(!strcmp(n,methods[i].name))return 0x7f000000+i*16;return 0;}
uint32_t hle_com_vtable(const win32hle_shim*s){unsigned n=0;while(s[n].name)++n;uint32_t*p=hle_alloc(n*4);win32hle_register(s);for(unsigned i=0;i<n;++i)p[i]=hle_resolve(s[i].name);return hle_va(p);}
uint32_t hle_com_new(uint32_t vt,uint32_t n){uint32_t*p=hle_alloc(n<8?8:n);p[0]=vt;p[1]=1;return hle_va(p);}
void hle_com_free(uint32_t o){(void)o;}
void hle_com_AddRef(void){RET(++MEM32(A32(0)+4),1);}
void hle_com_Release(void){RET(--MEM32(A32(0)+4),1);}
uint32_t hle_scratch_mark(void){return next_alloc;}
uint32_t hle_scratch(uint32_t n){return hle_va(hle_alloc(n));}
void hle_scratch_release(uint32_t m){next_alloc=m;}
void hle_input_key(int d,uint32_t vk,uint32_t sc){(void)d;(void)vk;(void)sc;}
void hle_input_mouse(uint32_t m,int x,int y){(void)m;(void)x;(void)y;}
uint32_t hle_first_hwnd(void){return 0;}
void hle_post_message(uint32_t h,uint32_t m,uint32_t w,uint32_t l){(void)h;(void)m;(void)w;(void)l;}
uint32_t hle_call_guest(uint32_t va,int n,const uint32_t*a){
    if(va==0x123456){assert(n==2);assert(MEM32(a[0])==580);assert(MEM32(a[0]+36)==0x15);memcpy(instance_guid,hle_ptr(a[0]+4),16);++enumerated;return 1;}
    assert(va>=0x7f000000);unsigned index=(va-0x7f000000)/16;assert(index<method_count);
    uint32_t saved=g_esp;g_esp=0x380000;MEM32(g_esp)=0xdeadbeef;for(int i=0;i<n;++i)MEM32(g_esp+4+i*4)=a[i];methods[index].fn();assert(g_esp==0x380004+(uint32_t)n*4);uint32_t result=g_eax;g_esp=saved;return result;
}
static uint32_t invoke(uint32_t self,unsigned slot,int extra,const uint32_t*args){uint32_t a[12]={self};for(int i=0;i<extra;++i)a[i+1]=args[i];return hle_call_guest(MEM32(MEM32(self)+slot*4),extra+1,a);}

void hle_com_QueryInterface(void){if(A32(2))MEM32(A32(2))=A32(0);++MEM32(A32(0)+4);RET(0,3);}
void hle_com_register_class(const uint8_t *c,uint32_t(*f)(const uint8_t*,uint32_t*)){(void)c;(void)f;}
#endif
