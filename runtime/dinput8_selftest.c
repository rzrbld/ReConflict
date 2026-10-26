/* Isolated DirectInput guest ABI test: real SDL virtual controller, minimal
 * guest-memory/shim harness. This does not execute game code. */
#include <SDL.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "win32hle.h"
extern const win32hle_shim ds_dinput8_shims[];
extern void ds_input_pump(void);
#include "guest_test_harness.h"
static int attach(void){int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,16,1);assert(index>=0);char guid[64],mapping[512];SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index),guid,sizeof guid);snprintf(mapping,sizeof mapping,"%s,DS test controller,a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,back:b6,start:b7,leftstick:b8,rightstick:b9,dpup:b10,dpright:b11,dpdown:b12,dpleft:b13,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,platform:Linux,",guid);assert(SDL_GameControllerAddMapping(mapping)>=0);return index;}
static char command_path[256];
static void append_commands(const char *text){FILE*f=fopen(command_path,"ab");assert(f);assert(fwrite(text,1,strlen(text),f)==strlen(text));assert(!fclose(f));}
int main(int argc,char**argv){
    int automation=argc==2&&!strcmp(argv[1],"--commands");char test_root[]="/tmp/ds-input-XXXXXX";
    if(automation){assert(mkdtemp(test_root));assert(!setenv("DS_ROOT",test_root,1));assert(!setenv("DS_TEST_INPUT","1",1));snprintf(command_path,sizeof command_path,"%s/input.commands",test_root);}

    g_mem_base=(ptrdiff_t)calloc(1,0x1000000);assert(g_mem_base);SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER));
    int index=attach();SDL_Joystick*j=SDL_JoystickOpen(index);assert(j);SDL_JoystickSetVirtualAxis(j,0,-32768);SDL_JoystickSetVirtualAxis(j,1,32767);SDL_JoystickSetVirtualAxis(j,2,16384);SDL_JoystickSetVirtualAxis(j,4,32767);SDL_JoystickSetVirtualAxis(j,5,-32768);SDL_JoystickSetVirtualButton(j,0,1);SDL_JoystickSetVirtualButton(j,7,1);SDL_JoystickSetVirtualButton(j,10,1);SDL_JoystickUpdate();
    win32hle_register(ds_dinput8_shims);uint32_t iid=hle_scratch(16),out=hle_scratch(4);const uint8_t iid_data[16]={0x30,0x80,0x79,0xbf,0x3a,0x48,0xa2,0x4d,0xaa,0x99,0x5d,0x64,0xed,0x36,0x97,0};memcpy(hle_ptr(iid),iid_data,16);
    uint32_t create[5]={0,0x800,iid,out,0};assert(!hle_call_guest(hle_resolve("DirectInput8Create"),5,create));uint32_t di=MEM32(out);assert(di);
    uint32_t en[4]={4,0x123456,0,1};assert(!invoke(di,4,4,en));assert(enumerated==1);uint32_t guid=hle_scratch(16);memcpy(hle_ptr(guid),instance_guid,16);uint32_t devargs[3]={guid,out,0};assert(!invoke(di,3,3,devargs));uint32_t d=MEM32(out);
    uint32_t format=hle_scratch(24),objects=hle_scratch(164*16),axisguid=hle_scratch(6*16);MEM32(format)=24;MEM32(format+4)=16;MEM32(format+8)=1;MEM32(format+12)=272;MEM32(format+16)=164;MEM32(format+20)=objects;
    uint32_t axis[6]={0xa36d02e0,0xa36d02e1,0xa36d02e2,0xa36d02f4,0xa36d02f5,0xa36d02e3};
    for(unsigned i=0;i<164;++i){uint32_t o=objects+i*16;if(i<8){uint32_t ag=axisguid+(i<6?i:0)*16;MEM32(ag)=axis[i<6?i:0];MEM32(o)=ag;MEM32(o+4)=i*4;MEM32(o+8)=0x80ffff03;MEM32(o+12)=0x100;}else if(i<12){MEM32(o+4)=32+(i-8)*4;MEM32(o+8)=0x80ffff10;}else if(i<140){MEM32(o+4)=48+i-12;MEM32(o+8)=0x80ffff0c;}else{MEM32(o)=axisguid;MEM32(o+4)=176+(i-140)*4;MEM32(o+8)=0x80ffff03;MEM32(o+12)=0x200;}}
    uint32_t fmtarg[1]={format};assert(!invoke(d,11,1,fmtarg));uint32_t state=hle_scratch(272),stateargs[2]={272,state};assert(invoke(d,9,2,stateargs)==0x8007000c);assert(!invoke(d,7,0,NULL));assert(!invoke(d,25,0,NULL));assert(!invoke(d,9,2,stateargs));
    assert((int32_t)MEM32(state)==-128);assert((int32_t)MEM32(state+4)==128);assert((int32_t)MEM32(state+8)>=45&&(int32_t)MEM32(state+8)<=46);assert(MEM8(state+48+2)==0x80);assert(MEM8(state+48+4)==0x80);assert(MEM8(state+48+5)==0);assert(MEM8(state+48+11)==0x80);assert(MEM8(state+48+12)==0x80);assert(MEM32(state+32)==UINT32_MAX);assert(MEM32(state+176)==0);
    if(automation){
        assert(!invoke(d,8,0,NULL));
        append_commands("tap a\ntap dpup\n");
        /* Pumping during loading must not consume taps without a valid state sample. */
        for(int i=0;i<20;++i)ds_input_pump();
        assert(invoke(d,9,2,stateargs)==0x8007000c);assert(!invoke(d,7,0,NULL));
        assert(!invoke(d,9,2,stateargs));assert(MEM8(state+50)==0x80);
        assert(!invoke(d,9,2,stateargs));assert(!MEM8(state+50));
        assert(!invoke(d,9,2,stateargs));assert(MEM8(state+60)==0x80);
        assert(!invoke(d,9,2,stateargs));assert(!MEM8(state+60));
        assert(!invoke(d,9,2,stateargs));assert(MEM8(state+50)==0x80);assert(MEM8(state+60)==0x80);
        append_commands("button a 0\naxis leftx 0\naxis lefty -24000\n");assert(!invoke(d,9,2,stateargs));assert(!MEM8(state+50));assert(!MEM32(state));assert((int32_t)MEM32(state+4)<-80);
        append_commands("button a ");assert(!invoke(d,9,2,stateargs));assert(!MEM8(state+50));
        append_commands("1\naxis lefty 0\n");assert(!invoke(d,9,2,stateargs));assert(MEM8(state+50)==0x80);assert(!MEM32(state+4));
        append_commands("button a 9\naxis lefty 40000\n");assert(!invoke(d,9,2,stateargs));assert(MEM8(state+50)==0x80);assert(!MEM32(state+4));
        append_commands("reset\n");assert(!invoke(d,9,2,stateargs));assert((int32_t)MEM32(state)==-128);assert((int32_t)MEM32(state+4)==128);
        FILE*f=fopen(command_path,"wb");assert(f);fclose(f);append_commands("button a 0\n");assert(!invoke(d,9,2,stateargs));assert(!MEM8(state+50));
    }
    SDL_JoystickClose(j);assert(!SDL_JoystickDetachVirtual(index));assert(invoke(d,25,0,NULL)==0x80040209);index=attach();assert(!invoke(d,25,0,NULL));assert(!invoke(d,9,2,stateargs));assert(MEM8(state+48+2)==0);assert(!invoke(d,8,0,NULL));assert(invoke(d,9,2,stateargs)==0x8007000c);SDL_JoystickDetachVirtual(index);SDL_Quit();if(automation){assert(!unlink(command_path));assert(!rmdir(test_root));puts("Opt-in command stream: holds, taps survive pumps, one pressed/released sample, partial lines, range rejection, reset, truncate: PASS");}free((void*)g_mem_base);puts("DirectInput8 guest ABI, 272-byte state, triggers, range, unplug/reconnect: PASS");return 0;
}
