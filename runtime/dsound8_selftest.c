/* Tests DS8 ABI extension against the real win32hle SDL PCM mixer, using
 * a silent SDL device. No original game or physical speaker is exercised. */
#include <SDL.h>
#include <stdio.h>
#include <math.h>
#include "guest_test_harness.h"
extern const win32hle_shim ds_dsound8_shims[];
static uint32_t float_bits(float f){uint32_t u;memcpy(&u,&f,4);return u;}
static uint32_t make_buffer(uint32_t ds,uint32_t flags){uint32_t desc=hle_scratch(36),out=hle_scratch(4),wfx=hle_scratch(18);MEM16(wfx)=1;MEM16(wfx+2)=1;MEM32(wfx+4)=22050;MEM32(wfx+8)=44100;MEM16(wfx+12)=2;MEM16(wfx+14)=16;MEM32(desc)=36;MEM32(desc+4)=flags;MEM32(desc+8)=4096;MEM32(desc+16)=(flags&1)?0:wfx;uint32_t a[3]={desc,out,0};assert(!invoke(ds,3,3,a));return MEM32(out);}
static uint32_t query(uint32_t object,const uint8_t*g){uint32_t guid=hle_scratch(16),out=hle_scratch(4);memcpy(hle_ptr(guid),g,16);uint32_t a[2]={guid,out};assert(!invoke(object,0,2,a));assert(MEM32(out));return MEM32(out);}
int main(void){
    g_mem_base=(ptrdiff_t)calloc(1,0x400000);assert(g_mem_base);SDL_setenv("SDL_AUDIODRIVER","dummy",1);win32hle_register(win32hle_dsound);win32hle_register(ds_dsound8_shims);
    uint32_t out=hle_scratch(4),create[3]={0,out,0};assert(!hle_call_guest(hle_resolve("DirectSoundCreate8"),3,create));uint32_t ds=MEM32(out);assert(ds);uint32_t certified[1]={out};assert(!invoke(ds,11,1,certified));assert(MEM32(out)==1);
    static const uint8_t li[16]={0x84,0xfa,0x9a,0x27,0x81,0x49,0xce,0x11,0xa5,0x21,0,0x20,0xaf,0x0b,0xe5,0x60};
    static const uint8_t bi[16]={0x86,0xfa,0x9a,0x27,0x81,0x49,0xce,0x11,0xa5,0x21,0,0x20,0xaf,0x0b,0xe5,0x60};
    uint32_t primary=make_buffer(ds,0x11),secondary=make_buffer(ds,0x90),listener=query(primary,li),spatial=query(secondary,bi);
    uint32_t position[4]={float_bits(0),float_bits(0),float_bits(10),1};assert(!invoke(spatial,19,4,position));uint32_t all=hle_scratch(64),get[1]={all};MEM32(all)=64;assert(!invoke(spatial,3,1,get));assert(MEM32(all+12)==float_bits(0));assert(!invoke(listener,17,0,NULL));MEM32(all)=64;assert(!invoke(spatial,3,1,get));assert(MEM32(all+12)==float_bits(10));
    uint32_t vol_arg[1]={(uint32_t)-600};assert(!invoke(secondary,15,1,vol_arg));assert(!invoke(secondary,6,1,certified));assert((int32_t)MEM32(out)==-600);
    uint32_t raw_volume[2]={secondary,out};assert(!hle_call_guest(hle_resolve("IDirectSoundBuffer::GetVolume"),2,raw_volume));assert((int32_t)MEM32(out)>=-2601&&(int32_t)MEM32(out)<=-2599);
    /* Move listener onto source: attenuation disappears, requested base volume remains. */
    uint32_t lpos[4]={float_bits(0),float_bits(0),float_bits(10),0};assert(!invoke(listener,14,4,lpos));assert(!invoke(secondary,6,1,certified));assert((int32_t)MEM32(out)==-600);
    assert(!hle_call_guest(hle_resolve("IDirectSoundBuffer::GetVolume"),2,raw_volume));assert((int32_t)MEM32(out)==-600);
    uint32_t fxargs[3]={1,0,0};assert(invoke(secondary,21,3,fxargs)==0x80004001u);
    uint32_t lockout=hle_scratch(16),lockargs[7]={0,4096,lockout,lockout+4,lockout+8,lockout+12,0};assert(!invoke(secondary,11,7,lockargs));assert(MEM32(lockout)&&MEM32(lockout+4)==4096);memset(hle_ptr(MEM32(lockout)),0,4096);uint32_t unlock[4]={MEM32(lockout),4096,0,0};assert(!invoke(secondary,19,4,unlock));uint32_t play[3]={0,0,1};assert(!invoke(secondary,12,3,play));SDL_Delay(30);uint32_t cursor[2]={lockout,lockout+4};assert(!invoke(secondary,4,2,cursor));assert(MEM32(lockout)>0);
    assert(invoke(spatial,2,0,NULL)==0);assert(invoke(listener,2,0,NULL)==0);assert(invoke(secondary,2,0,NULL)==0);assert(invoke(primary,2,0,NULL)==0);assert(invoke(ds,2,0,NULL)==0);SDL_Quit();free((void*)g_mem_base);puts("DirectSound8 COM, 3D deferred state, volume, PCM lock/play/cursors and reference lifetime: PASS");return 0;
}
