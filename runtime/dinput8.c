/* DirectInput 8 guest COM bridge. Win32 pointers/vtables remain 32-bit;
 * SDL owns physical devices, while the game receives its original PS2 layout.
 * API layout reference: Wine include/dinput.h (no implementation copied). */
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "win32hle.h"
#include "../probe/desert_pad.h"
#include "test_input.h"

#define DI_OK 0u
#define E_NOINTERFACE 0x80004002u
#define DIERR_INVALIDPARAM 0x80070057u
#define DIERR_NOTACQUIRED 0x8007000cu
#define DIERR_NOTFOUND 0x80070002u
#define DIERR_UNSUPPORTED 0x80004001u
#define DIERR_UNPLUGGED 0x80040209u
#define MAX_DEVICES 32
enum { K_KEYBOARD=1, K_MOUSE=2, K_PAD=3 };
typedef struct { uint32_t offset, type, guid, flags; } FormatObject;
typedef struct {
    int used, kind, ordinal, acquired, relative;
    uint32_t self, datasize, buffer_size, hwnd, format_flags;
    int32_t low[6], high[6];
    SDL_GameController *pad;
    SDL_JoystickID instance;
    uint8_t guid[16];
    DS_PadConfig config;
    FormatObject format[256];
    unsigned objects;
    int traced;
    DS_PadState previous;
    uint32_t previous_game[7];
} InputDevice;
static InputDevice devices[MAX_DEVICES];
static uint32_t vt_input, vt_device;
static int initialized;
static uint8_t keyboard[256];
static int mouse_dx, mouse_dy, mouse_dz;
static uint8_t mouse_buttons[8];
static uint32_t sequence;

static const uint8_t iid_input8a[16]={0x30,0x80,0x79,0xbf,0x3a,0x48,0xa2,0x4d,0xaa,0x99,0x5d,0x64,0xed,0x36,0x97,0};
static const uint8_t iid_device8a[16]={0x80,0x10,0xd4,0x54,0x15,0xdc,0x33,0x48,0xa4,0x1b,0x74,0x8f,0x73,0xa3,0x81,0x79};
static const uint8_t iid_unknown[16]={0,0,0,0,0,0,0,0,0xc0,0,0,0,0,0,0,0x46};

static unsigned dik(SDL_Scancode s) {
    static const uint8_t letters[26]={0x1e,0x30,0x2e,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,0x31,0x18,0x19,0x10,0x13,0x1f,0x14,0x16,0x2f,0x11,0x2d,0x15,0x2c};
    if(s>=SDL_SCANCODE_A&&s<=SDL_SCANCODE_Z)return letters[s-SDL_SCANCODE_A];
    if(s>=SDL_SCANCODE_1&&s<=SDL_SCANCODE_9)return 2u+(unsigned)(s-SDL_SCANCODE_1);
    if(s>=SDL_SCANCODE_F1&&s<=SDL_SCANCODE_F10)return 0x3bu+(unsigned)(s-SDL_SCANCODE_F1);
    switch(s){
    case SDL_SCANCODE_0:return 0x0b; case SDL_SCANCODE_ESCAPE:return 1; case SDL_SCANCODE_RETURN:return 0x1c;
    case SDL_SCANCODE_TAB:return 0xf;case SDL_SCANCODE_BACKSPACE:return 0xe;case SDL_SCANCODE_SPACE:return 0x39;
    case SDL_SCANCODE_LCTRL:return 0x1d;case SDL_SCANCODE_RCTRL:return 0x9d;case SDL_SCANCODE_LSHIFT:return 0x2a;case SDL_SCANCODE_RSHIFT:return 0x36;
    case SDL_SCANCODE_LALT:return 0x38;case SDL_SCANCODE_RALT:return 0xb8;
    case SDL_SCANCODE_MINUS:return 0x0c;case SDL_SCANCODE_EQUALS:return 0x0d;case SDL_SCANCODE_LEFTBRACKET:return 0x1a;case SDL_SCANCODE_RIGHTBRACKET:return 0x1b;
    case SDL_SCANCODE_SEMICOLON:return 0x27;case SDL_SCANCODE_APOSTROPHE:return 0x28;case SDL_SCANCODE_GRAVE:return 0x29;case SDL_SCANCODE_BACKSLASH:return 0x2b;
    case SDL_SCANCODE_COMMA:return 0x33;case SDL_SCANCODE_PERIOD:return 0x34;case SDL_SCANCODE_SLASH:return 0x35;
    case SDL_SCANCODE_CAPSLOCK:return 0x3a;case SDL_SCANCODE_NUMLOCKCLEAR:return 0x45;case SDL_SCANCODE_SCROLLLOCK:return 0x46;
    case SDL_SCANCODE_KP_7:return 0x47;case SDL_SCANCODE_KP_8:return 0x48;case SDL_SCANCODE_KP_9:return 0x49;case SDL_SCANCODE_KP_MINUS:return 0x4a;
    case SDL_SCANCODE_KP_4:return 0x4b;case SDL_SCANCODE_KP_5:return 0x4c;case SDL_SCANCODE_KP_6:return 0x4d;case SDL_SCANCODE_KP_PLUS:return 0x4e;
    case SDL_SCANCODE_KP_1:return 0x4f;case SDL_SCANCODE_KP_2:return 0x50;case SDL_SCANCODE_KP_3:return 0x51;case SDL_SCANCODE_KP_0:return 0x52;case SDL_SCANCODE_KP_PERIOD:return 0x53;
    case SDL_SCANCODE_F11:return 0x57;case SDL_SCANCODE_F12:return 0x58;case SDL_SCANCODE_KP_ENTER:return 0x9c;case SDL_SCANCODE_KP_DIVIDE:return 0xb5;
    case SDL_SCANCODE_HOME:return 0xc7;case SDL_SCANCODE_UP:return 0xc8;case SDL_SCANCODE_PAGEUP:return 0xc9;case SDL_SCANCODE_LEFT:return 0xcb;
    case SDL_SCANCODE_RIGHT:return 0xcd;case SDL_SCANCODE_END:return 0xcf;case SDL_SCANCODE_DOWN:return 0xd0;case SDL_SCANCODE_PAGEDOWN:return 0xd1;
    case SDL_SCANCODE_INSERT:return 0xd2;case SDL_SCANCODE_DELETE:return 0xd3; default:return 0;}
}
static uint32_t vk(SDL_Keysym k) {
    if(k.sym>=SDLK_a&&k.sym<=SDLK_z)return (uint32_t)(k.sym-SDLK_a+'A');
    if(k.sym>=SDLK_0&&k.sym<=SDLK_9)return (uint32_t)k.sym;
    if(k.sym>=SDLK_F1&&k.sym<=SDLK_F12)return 0x70u+(uint32_t)(k.sym-SDLK_F1);
    switch(k.sym){case SDLK_UP:return 0x26;case SDLK_DOWN:return 0x28;case SDLK_LEFT:return 0x25;case SDLK_RIGHT:return 0x27;
    case SDLK_ESCAPE:return 27;case SDLK_RETURN:return 13;case SDLK_SPACE:return 32;case SDLK_TAB:return 9;case SDLK_BACKSPACE:return 8;
    case SDLK_LSHIFT:case SDLK_RSHIFT:return 16;case SDLK_LCTRL:case SDLK_RCTRL:return 17;case SDLK_LALT:case SDLK_RALT:return 18;
    default:return (k.sym>0&&k.sym<128)?(uint32_t)k.sym:0;}
}
void ds_input_pump(void) {
    extern void ds_runtime_poll(void) __attribute__((weak));
    if(ds_runtime_poll)ds_runtime_poll();
    if(!(SDL_WasInit(SDL_INIT_EVENTS)&SDL_INIT_EVENTS))return;
    SDL_Event e;
    while(SDL_PollEvent(&e)){
        if(e.type==SDL_KEYDOWN||e.type==SDL_KEYUP){unsigned key=dik(e.key.keysym.scancode);if(key)keyboard[key]=e.type==SDL_KEYDOWN?0x80:0;uint32_t keyvk=vk(e.key.keysym);if(keyvk)hle_input_key(e.type==SDL_KEYDOWN,keyvk,key);}
        else if(e.type==SDL_MOUSEMOTION){mouse_dx+=e.motion.xrel;mouse_dy+=e.motion.yrel;hle_input_mouse(0x200,e.motion.x,e.motion.y);}
        else if(e.type==SDL_MOUSEWHEEL)mouse_dz+=e.wheel.y*120;
        else if(e.type==SDL_MOUSEBUTTONDOWN||e.type==SDL_MOUSEBUTTONUP){unsigned b=e.button.button==SDL_BUTTON_LEFT?0:e.button.button==SDL_BUTTON_RIGHT?1:e.button.button==SDL_BUTTON_MIDDLE?2:7;mouse_buttons[b]=e.type==SDL_MOUSEBUTTONDOWN?0x80:0;uint32_t message=b==0?(e.type==SDL_MOUSEBUTTONDOWN?0x201:0x202):b==1?(e.type==SDL_MOUSEBUTTONDOWN?0x204:0x205):(e.type==SDL_MOUSEBUTTONDOWN?0x207:0x208);hle_input_mouse(message,e.button.x,e.button.y);}
        else if(e.type==SDL_QUIT){uint32_t w=hle_first_hwnd();if(w)hle_post_message(w,0x10,0,0);}
        else if(e.type==SDL_WINDOWEVENT&&(e.window.event==SDL_WINDOWEVENT_FOCUS_LOST||e.window.event==SDL_WINDOWEVENT_FOCUS_GAINED)){
            int gained=e.window.event==SDL_WINDOWEVENT_FOCUS_GAINED;
            if(!gained){memset(keyboard,0,sizeof keyboard);memset(mouse_buttons,0,sizeof mouse_buttons);}
            uint32_t w=hle_first_hwnd();if(w){hle_post_message(w,0x6,gained?1:0,0);hle_post_message(w,gained?0x7:0x8,0,0);}
        }
        ++sequence;
    }
    SDL_GameControllerUpdate();
    ds_test_input_pump();
}
static int ensure_sdl(void) {
    if(initialized)return 1;
    if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)<0){fprintf(stderr,"[dinput8] SDL: %s\n",SDL_GetError());return 0;}
    initialized=1;return 1;
}
static InputDevice *device(uint32_t self){for(unsigned i=0;i<MAX_DEVICES;++i)if(devices[i].used&&devices[i].self==self)return &devices[i];return NULL;}
static void guid(uint8_t *out,int kind,int ordinal){static const uint8_t tail[12]={0xa0,0xd5,0xcf,0x11,0xbf,0xc7,0x44,0x45,0x53,0x54,0,0};uint32_t head=kind==K_KEYBOARD?0x6f1d2b61u:kind==K_MOUSE?0x6f1d2b60u:0x44530000u+(uint32_t)ordinal;memcpy(out,&head,4);memcpy(out+4,tail,12);}
static int pad_index(int ordinal){int found=0;for(int i=0;i<SDL_NumJoysticks();++i)if(SDL_IsGameController(i)){if(found++==ordinal)return i;}return -1;}
static SDL_GameController *get_pad(InputDevice *d){if(d->pad&&SDL_GameControllerGetAttached(d->pad))return d->pad;if(d->pad)SDL_GameControllerClose(d->pad);d->pad=NULL;int i=pad_index(d->ordinal);if(i>=0){d->pad=SDL_GameControllerOpen(i);if(d->pad)d->instance=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(d->pad));}return d->pad;}
static int identify(const uint8_t *g,int *ordinal){if(!g)return 0;uint32_t h;memcpy(&h,g,4);*ordinal=0;if(h==0x6f1d2b61u)return K_KEYBOARD;if(h==0x6f1d2b60u)return K_MOUSE;if((h&0xffff0000u)==0x44530000u){*ordinal=(int)(h&0xffffu);return K_PAD;}return 0;}
static void info(uint32_t va,InputDevice *d,int kind,int ordinal){uint32_t n=MEM32(va);if(n<560||n>580)return;memset(hle_ptr(va),0,n);MEM32(va)=n;uint8_t g[16];guid(g,kind,ordinal);memcpy(hle_ptr(va+4),g,16);memcpy(hle_ptr(va+20),g,16);MEM32(va+36)=kind==K_KEYBOARD?0x13u:kind==K_MOUSE?0x12u:0x15u;const char *name=kind==K_KEYBOARD?"SDL2 Keyboard":kind==K_MOUSE?"SDL2 Mouse":"SDL2 Controller";if(kind==K_PAD){int index=pad_index(ordinal);if(index>=0&&SDL_GameControllerNameForIndex(index))name=SDL_GameControllerNameForIndex(index);}size_t len=strnlen(name,259);uint8_t *dst=hle_ptr(va);if(!dst)return;memcpy(dst+40,name,len);memcpy(dst+300,name,len);if(n>=580){MEM16(va+576)=1;MEM16(va+578)=kind==K_PAD?5:kind==K_MOUSE?2:6;}if(d)memcpy(d->guid,g,16);}
static void query_input(void){if(A32(1)&&A32(2)&&(!memcmp(APTR(1),iid_unknown,16)||!memcmp(APTR(1),iid_input8a,16))){MEM32(A32(2))=A32(0);++MEM32(A32(0)+4);RET(DI_OK,3);}if(A32(2))MEM32(A32(2))=0;RET(E_NOINTERFACE,3);}
static void query_device(void){if(A32(1)&&A32(2)&&(!memcmp(APTR(1),iid_unknown,16)||!memcmp(APTR(1),iid_device8a,16))){MEM32(A32(2))=A32(0);++MEM32(A32(0)+4);RET(DI_OK,3);}if(A32(2))MEM32(A32(2))=0;RET(E_NOINTERFACE,3);}
static void release_device(void){uint32_t self=A32(0),refs=--MEM32(self+4);if(!refs){InputDevice*d=device(self);if(d){if(d->pad)SDL_GameControllerClose(d->pad);memset(d,0,sizeof *d);}hle_com_free(self);}RET(refs,1);}
static void create_device(void){int ordinal;int kind=identify(APTR(1),&ordinal);uint32_t out=A32(2);if(out)MEM32(out)=0;if(!out||!kind||A32(3))RET(DIERR_INVALIDPARAM,4);if(kind==K_PAD&&pad_index(ordinal)<0)RET(DIERR_NOTFOUND,4);InputDevice*d=NULL;for(unsigned i=0;i<MAX_DEVICES;++i)if(!devices[i].used){d=&devices[i];break;}if(!d)RET(0x8007000eu,4);memset(d,0,sizeof *d);d->used=1;d->kind=kind;d->ordinal=ordinal;d->datasize=kind==K_KEYBOARD?256:kind==K_MOUSE?16:272;d->self=hle_com_new(vt_device,12);ds_pad_defaults(&d->config);const char *path=SDL_getenv("DS_PAD_CONFIG");if(path)ds_pad_load(&d->config,path);for(int i=0;i<6;++i){d->low[i]=-128;d->high[i]=128;}guid(d->guid,kind,ordinal);MEM32(out)=d->self;fprintf(stderr,"[dinput8] CreateDevice kind=%d ordinal=%d object=%#x\n",kind,ordinal,d->self);RET(DI_OK,4);}
static void enumerate(void){uint32_t type=A32(1),callback=A32(2),ref=A32(3);if(!callback)RET(DIERR_INVALIDPARAM,5);ds_input_pump();uint32_t mark=hle_scratch_mark(),instance=hle_scratch(580);int stop=0;for(int kind=K_KEYBOARD;kind<=K_PAD&&!stop;++kind){if(type&&!(type==1||(type==2&&kind==K_MOUSE)||(type==3&&kind==K_KEYBOARD)||(type==4&&kind==K_PAD)||(type==0x12&&kind==K_MOUSE)||(type==0x13&&kind==K_KEYBOARD)||(type==0x15&&kind==K_PAD)))continue;int n=kind==K_PAD?SDL_NumJoysticks():1;for(int ord=0;ord<n;++ord){if(kind==K_PAD&&pad_index(ord)<0)break;MEM32(instance)=580;info(instance,NULL,kind,ord);uint32_t args[2]={instance,ref};if(!hle_call_guest(callback,2,args)){stop=1;break;}}}hle_scratch_release(mark);RET(DI_OK,5);}
static void status(void){int ordinal;int k=identify(APTR(1),&ordinal);RET(k&&(k!=K_PAD||pad_index(ordinal)>=0)?DI_OK:DIERR_NOTFOUND,2);}
static void get_caps(void){InputDevice*d=device(A32(0));uint32_t p=A32(1);if(!d||!p||MEM32(p)<24)RET(DIERR_INVALIDPARAM,2);uint32_t n=MEM32(p);if(n>44)n=44;memset(hle_ptr(p+4),0,n-4);MEM32(p+4)=1;MEM32(p+8)=d->kind==K_PAD?0x15:d->kind==K_MOUSE?0x12:0x13;MEM32(p+12)=d->kind==K_PAD?4:d->kind==K_MOUSE?3:0;MEM32(p+16)=d->kind==K_PAD?16:d->kind==K_MOUSE?8:256;MEM32(p+20)=d->kind==K_PAD?1:0;RET(DI_OK,2);}
static void set_format(void){InputDevice*d=device(A32(0));uint32_t p=A32(1);if(!d||!p||MEM32(p)<24||MEM32(p+4)<16||MEM32(p+12)>4096||MEM32(p+16)>256)RET(DIERR_INVALIDPARAM,2);d->datasize=MEM32(p+12);d->objects=MEM32(p+16);d->format_flags=MEM32(p+8);uint32_t objects=MEM32(p+20),stride=MEM32(p+4);for(unsigned i=0;i<d->objects;++i){uint32_t o=objects+i*stride;d->format[i]=(FormatObject){MEM32(o+4),MEM32(o+8),MEM32(o)?MEM32(MEM32(o)):0,MEM32(o+12)};}RET(DI_OK,2);}
static void acquire(void){InputDevice*d=device(A32(0));if(!d)RET(DIERR_INVALIDPARAM,1);if(d->kind==K_PAD&&!get_pad(d))RET(DIERR_UNPLUGGED,1);d->acquired=1;RET(DI_OK,1);}
static void unacquire(void){InputDevice*d=device(A32(0));if(!d)RET(DIERR_INVALIDPARAM,1);d->acquired=0;RET(DI_OK,1);}
static void set_coop(void){InputDevice*d=device(A32(0));if(!d)RET(DIERR_INVALIDPARAM,3);d->hwnd=A32(1);RET(DI_OK,3);}
static int property_axis(uint32_t p){if(MEM32(p+12)==1){unsigned o=MEM32(p+8);return o<24&&o%4==0?(int)(o/4):-1;}return -1;}
static void set_property(void){InputDevice*d=device(A32(0));uint32_t prop=A32(1),p=A32(2);if(!d||!p||MEM32(p)<20||MEM32(p+4)!=16)RET(DIERR_INVALIDPARAM,3);if(prop==1)d->buffer_size=MEM32(p+16);else if(prop==2)d->relative=MEM32(p+16)!=0;else if(prop==4){if(MEM32(p)<24)RET(DIERR_INVALIDPARAM,3);int axis=property_axis(p);if(axis<0){for(int i=0;i<6;++i){d->low[i]=(int32_t)MEM32(p+16);d->high[i]=(int32_t)MEM32(p+20);}}else{d->low[axis]=(int32_t)MEM32(p+16);d->high[axis]=(int32_t)MEM32(p+20);}}else if(prop==5){uint32_t dz=MEM32(p+16);if(dz>10000)RET(DIERR_INVALIDPARAM,3);d->config.deadzone=dz/10000.f;}else if(prop==6||prop==7||prop==8){/* Saturation/granularity are not adjusted: report unsupported. */RET(DIERR_UNSUPPORTED,3);}else RET(DIERR_UNSUPPORTED,3);RET(DI_OK,3);}
static void get_property(void){InputDevice*d=device(A32(0));uint32_t prop=A32(1),p=A32(2);if(!d||!p||MEM32(p)<20)RET(DIERR_INVALIDPARAM,3);if(prop==1)MEM32(p+16)=d->buffer_size;else if(prop==2)MEM32(p+16)=d->relative;else if(prop==4&&MEM32(p)>=24){int a=property_axis(p);if(a<0)a=0;MEM32(p+16)=(uint32_t)d->low[a];MEM32(p+20)=(uint32_t)d->high[a];}else if(prop==5)MEM32(p+16)=(uint32_t)(d->config.deadzone*10000);else RET(DIERR_UNSUPPORTED,3);RET(DI_OK,3);}
static int source_offset(const FormatObject*f,int kind){unsigned type=f->type&0xff,instance=(f->type>>8)&0xffff;if(type&0x0c){if(instance==0xffff)return (int)f->offset;return kind==K_KEYBOARD?(int)instance:kind==K_MOUSE?(int)(12+instance):(int)(48+instance);}if(type&3){if((f->flags&0xf00)&&((f->flags&0xf00)!=0x100))return (int)f->offset;switch(f->guid){case 0xa36d02e0u:return 0;case 0xa36d02e1u:return 4;case 0xa36d02e2u:return 8;case 0xa36d02f4u:return 12;case 0xa36d02f5u:return 16;case 0xa36d02e3u:return 20;default:return instance<6?(int)(instance*4):-1;}}if(type&0x10)return 32+(int)(instance<4?instance*4:0);return -1;}
/* These addresses are reviewed against this game's original input code and
 * are read only. Logical state is from BEFORE the game consumes this sample. */
static void trace_pad(InputDevice*d,const DS_PadState*p){
    static int enabled=-1;if(enabled<0){const char*e=getenv("DS_TRACE_INPUT");enabled=e&&!strcmp(e,"1");}
    if(!enabled)return;
    uint32_t game[]={MEM32(0x7541c0),MEM32(0x60f744),MEM32(0x60f740),MEM32(0x60f730),MEM32(0x60f734),MEM32(0x617c18),MEM16(0x610798)};
    if(d->traced&&!memcmp(&d->previous,p,sizeof *p)&&!memcmp(d->previous_game,game,sizeof game))return;
    unsigned buttons=0;for(int b=0;b<16;++b)if(p->buttons[b])buttons|=1u<<b;
    fprintf(stderr,"[dinput8-state] tick=%u device=%d%s buttons=%#x x=%d y=%d z=%d rz=%d pregameinputupdate count=%u mask=%#x joyidx=%d logicalA=%#x forward=%#x back=%#x up=%#x down=%#x frontend=%#x players=%u\n",
        SDL_GetTicks(),d->ordinal,d->traced?"":" initial",buttons,p->x,p->y,p->z,p->rz,
        MEM32(0x754be8),MEM32(0x60f5f0),(int32_t)MEM32(0x60f980),game[0],game[1],game[2],game[3],game[4],game[5],game[6]);
    d->previous=*p;memcpy(d->previous_game,game,sizeof game);d->traced=1;
}
static void get_state(void){InputDevice*d=device(A32(0));uint32_t n=A32(1),out=A32(2);if(!d||!out||n!=d->datasize||n>4096)RET(DIERR_INVALIDPARAM,3);if(!d->acquired)RET(DIERR_NOTACQUIRED,3);ds_input_pump();uint8_t state[272]={0};size_t source_size=sizeof state;if(d->kind==K_PAD){DS_PadState p;SDL_GameController*pad=get_pad(d);if(!pad)RET(DIERR_UNPLUGGED,3);ds_test_input_sample(pad,&d->config,&p);trace_pad(d,&p);memcpy(state,&p,sizeof p);for(int i=0;i<6;++i){int32_t v;memcpy(&v,state+i*4,4);double scaled=d->low[i]+((double)v+128)*(d->high[i]-d->low[i])/256.;v=(int32_t)scaled;memcpy(state+i*4,&v,4);}}else if(d->kind==K_KEYBOARD){memcpy(state,keyboard,256);source_size=256;}else{memcpy(state,&mouse_dx,4);memcpy(state+4,&mouse_dy,4);memcpy(state+8,&mouse_dz,4);memcpy(state+12,mouse_buttons,8);source_size=20;mouse_dx=mouse_dy=mouse_dz=0;}memset(hle_ptr(out),0,n);if(!d->objects){memcpy(hle_ptr(out),state,n<source_size?n:source_size);}else for(unsigned i=0;i<d->objects;++i){const FormatObject*f=&d->format[i];int from=source_offset(f,d->kind);unsigned bytes=(f->type&0x0c)?1:4;if(from>=0&&(size_t)from+bytes<=source_size&&f->offset+bytes<=n)memcpy(hle_ptr(out+f->offset),state+from,bytes);}RET(DI_OK,3);}
static void get_data(void){/* Buffered input is optional; fail instead of losing transitions silently. */InputDevice*d=device(A32(0));if(!d||!A32(3))RET(DIERR_INVALIDPARAM,5);MEM32(A32(3))=0;RET(d->buffer_size?DIERR_UNSUPPORTED:0x80070015u,5);}
static void device_info(void){InputDevice*d=device(A32(0));if(!d||!A32(1)||MEM32(A32(1))<560||MEM32(A32(1))>580)RET(DIERR_INVALIDPARAM,2);info(A32(1),d,d->kind,d->ordinal);RET(DI_OK,2);}
static void poll(void){InputDevice*d=device(A32(0));if(!d)RET(DIERR_INVALIDPARAM,1);ds_input_pump();if(d->kind==K_PAD&&!get_pad(d))RET(DIERR_UNPLUGGED,1);RET(d->acquired?DI_OK:DIERR_NOTACQUIRED,1);}
static void unsupported2(void){RET(DIERR_UNSUPPORTED,2);}static void unsupported3(void){RET(DIERR_UNSUPPORTED,3);}static void unsupported4(void){RET(DIERR_UNSUPPORTED,4);}static void unsupported5(void){RET(DIERR_UNSUPPORTED,5);}static void unsupported6(void){RET(DIERR_UNSUPPORTED,6);}
static void event_notification(void){if(A32(1))RET(DIERR_UNSUPPORTED,2);RET(DI_OK,2);}
static void initialize_input(void){RET(DI_OK,3);}static void initialize_device(void){RET(DI_OK,4);}
static void create_effect(void){if(A32(3))MEM32(A32(3))=0;RET(DIERR_UNSUPPORTED,5);}
static void enum_effects(void){RET(DI_OK,4);}static void ff_state(void){if(A32(1))MEM32(A32(1))=0;RET(DIERR_UNSUPPORTED,2);}
static void vtables(void){if(vt_input)return;static const win32hle_shim input[]={
{"DS_DI8::QueryInterface",query_input},{"DS_DI8::AddRef",hle_com_AddRef},{"DS_DI8::Release",hle_com_Release},{"DS_DI8::CreateDevice",create_device},{"DS_DI8::EnumDevices",enumerate},{"DS_DI8::GetDeviceStatus",status},{"DS_DI8::RunControlPanel",unsupported3},{"DS_DI8::Initialize",initialize_input},{"DS_DI8::FindDevice",unsupported4},{"DS_DI8::EnumDevicesBySemantics",unsupported6},{"DS_DI8::ConfigureDevices",unsupported4},{0,0}};
static const win32hle_shim dev[]={
{"DS_DID8::QueryInterface",query_device},{"DS_DID8::AddRef",hle_com_AddRef},{"DS_DID8::Release",release_device},{"DS_DID8::GetCapabilities",get_caps},{"DS_DID8::EnumObjects",unsupported4},{"DS_DID8::GetProperty",get_property},{"DS_DID8::SetProperty",set_property},{"DS_DID8::Acquire",acquire},{"DS_DID8::Unacquire",unacquire},{"DS_DID8::GetDeviceState",get_state},{"DS_DID8::GetDeviceData",get_data},{"DS_DID8::SetDataFormat",set_format},{"DS_DID8::SetEventNotification",event_notification},{"DS_DID8::SetCooperativeLevel",set_coop},{"DS_DID8::GetObjectInfo",unsupported4},{"DS_DID8::GetDeviceInfo",device_info},{"DS_DID8::RunControlPanel",unsupported3},{"DS_DID8::Initialize",initialize_device},{"DS_DID8::CreateEffect",create_effect},{"DS_DID8::EnumEffects",enum_effects},{"DS_DID8::GetEffectInfo",unsupported3},{"DS_DID8::GetForceFeedbackState",ff_state},{"DS_DID8::SendForceFeedbackCommand",unsupported2},{"DS_DID8::EnumCreatedEffectObjects",unsupported4},{"DS_DID8::Escape",unsupported2},{"DS_DID8::Poll",poll},{"DS_DID8::SendDeviceData",unsupported5},{"DS_DID8::EnumEffectsInFile",unsupported5},{"DS_DID8::WriteEffectToFile",unsupported5},{"DS_DID8::BuildActionMap",unsupported4},{"DS_DID8::SetActionMap",unsupported4},{"DS_DID8::GetImageInfo",unsupported2},{0,0}};vt_input=hle_com_vtable(input);vt_device=hle_com_vtable(dev);}
static void direct_input_create(void){uint32_t out=A32(3);if(out)MEM32(out)=0;if(!out||!A32(2)||A32(4))RET(DIERR_INVALIDPARAM,5);if(memcmp(APTR(2),iid_input8a,16))RET(E_NOINTERFACE,5);if(!ensure_sdl())RET(0x80004005u,5);vtables();MEM32(out)=hle_com_new(vt_input,8);fprintf(stderr,"[dinput8] initialized SDL2 backend\n");RET(DI_OK,5);}
const win32hle_shim ds_dinput8_shims[]={{"DirectInput8Create",direct_input_create},{0,0}};
