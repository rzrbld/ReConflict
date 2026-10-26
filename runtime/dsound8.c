/* IDirectSound8/Buffer8 guest ABI extensions over win32hle's SDL PCM mixer.
 * 3D listener/buffer state is applied as software distance/cone attenuation,
 * stereo pan and Doppler. Unsupported DSP effects return E_NOTIMPL.
 * Interface layout reference: Wine include/dsound.h; no Wine implementation
 * code is copied here. */
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "win32hle.h"
#define DS_OK 0u
#define E_NOINTERFACE 0x80004002u
#define E_NOTIMPL 0x80004001u
#define E_INVALIDARG 0x80070057u
#define E_OUTOFMEMORY 0x8007000eu
typedef struct {float x,y,z;} V3;
typedef struct {uint32_t size;V3 position,velocity;uint32_t inside,outside;V3 cone;int32_t outside_volume;float min_distance,max_distance;uint32_t mode;} B3;
typedef struct {uint32_t size;V3 position,velocity,front,top;float distance,rolloff,doppler;} L3;
_Static_assert(sizeof(B3)==64,"DS3DBUFFER ABI");
_Static_assert(sizeof(L3)==64,"DS3DLISTENER ABI");
typedef struct {uint32_t owner,flags;int used;int32_t volume,pan;uint32_t rate,original_rate;B3 current,pending;int dirty;} Buffer;
static Buffer buffers[256];
static L3 listener={64,{0,0,0},{0,0,0},{0,0,1},{0,1,0},1,1,1},pending_listener;
static int listener_dirty;
static uint32_t device_vt,buffer_vt,listener_vt,spatial_vt;
static uint32_t device_base[11],buffer_base[21];
static const uint8_t guid_tail[12]={0x81,0x49,0xce,0x11,0xa5,0x21,0,0x20,0xaf,0x0b,0xe5,0x60};
static const uint8_t iid_unknown[16]={0,0,0,0,0,0,0,0,0xc0,0,0,0,0,0,0,0x46};
static const uint8_t iid_ds8[16]={0x93,0x7e,0x0a,0xc5,0x95,0xf3,0x34,0x48,0x9e,0xf6,0x7f,0xa9,0x9d,0xe5,0x09,0x66};
static const uint8_t iid_buf8[16]={0x49,0xa4,0x25,0x68,0x24,0x75,0x82,0x4d,0x92,0x0f,0x50,0xe3,0x6a,0xb3,0xab,0x1e};
static int iid(uint32_t va,uint32_t first){return va&&MEM32(va)==first&&!memcmp(hle_ptr(va+4),guid_tail,12);}
static int unknown(uint32_t va){return va&&!memcmp(hle_ptr(va),iid_unknown,16);}
static int exact(uint32_t va,const uint8_t*g){return va&&!memcmp(hle_ptr(va),g,16);}
static float flt(uint32_t u){float f;memcpy(&f,&u,4);return f;}
static uint32_t bits(float f){uint32_t u;memcpy(&u,&f,4);return u;}
static Buffer*meta(uint32_t owner){for(unsigned i=0;i<256;++i)if(buffers[i].used&&buffers[i].owner==owner)return &buffers[i];return NULL;}
static Buffer*spatial(void){return meta(MEM32(A32(0)+8));}
static uint32_t invoke(uint32_t target,uint32_t owner,int n,const uint32_t*a){uint32_t args[12]={owner};for(int i=0;i<n;++i)args[i+1]=a[i];return hle_call_guest(target,n+1,args);}
static V3 sub(V3 a,V3 b){return (V3){a.x-b.x,a.y-b.y,a.z-b.z};}
static float dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static float length(V3 a){return sqrtf(dot(a,a));}
static V3 normalized(V3 a){float l=length(a);return l>1e-12f?(V3){a.x/l,a.y/l,a.z/l}:(V3){0,0,0};}
static V3 cross(V3 a,V3 b){return (V3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static int goodvec(V3 a){return isfinite(a.x)&&isfinite(a.y)&&isfinite(a.z);}
static void apply(Buffer*b){
    if(!b||!buffer_base[15])return;
    int32_t volume=b->volume,pan=b->pan;uint32_t rate=b->rate;
    if((b->flags&0x10)&&b->current.mode!=2){
        B3*s=&b->current;V3 delta=s->mode==1?s->position:sub(s->position,listener.position);float distance=length(delta),d=fminf(distance,s->max_distance);float minimum=fmaxf(s->min_distance,0.0001f);
        float gain=d>minimum?1.f/(1.f+listener.rolloff*(d/minimum-1.f)):1.f;
        if((b->flags&0x20000)&&distance>=s->max_distance)gain=0;
        if(s->outside<360&&length(s->cone)>0&&distance>0){float c=-dot(normalized(s->cone),normalized(delta));c=fminf(1,fmaxf(-1,c));float angle=acosf(c)*57.2957795f,inside=s->inside*.5f,outside=s->outside*.5f;if(angle>inside){float mix=outside>inside?fminf(1,(angle-inside)/(outside-inside)):1;gain*=powf(10,s->outside_volume*mix/2000.f);}}
        volume+=gain>0?(int32_t)(2000.f*log10f(gain)):-10000;
        V3 right=s->mode==1?(V3){1,0,0}:normalized(cross(listener.top,listener.front));float p=distance>0?dot(delta,right)/distance:0;p=fminf(.99999f,fmaxf(-.99999f,p));pan=(int32_t)((p<0?-1:1)*-1000.f*log10f(1-fabsf(p)));
        if(listener.doppler>0&&distance>0){V3 unit=normalized(delta);float lv=s->mode==1?0:dot(listener.velocity,unit);float sv=dot(s->velocity,unit);float num=343.5f+lv*listener.distance*listener.doppler,den=343.5f+sv*listener.distance*listener.doppler;float ratio=den>1?num/den:2;ratio=fmaxf(.5f,fminf(2,ratio));rate=(uint32_t)(rate*ratio);}
    }
    if(volume < -10000)volume=-10000;
    if(volume>0)volume=0;
    if(pan < -10000)pan=-10000;
    if(pan>10000)pan=10000;
    uint32_t v=(uint32_t)volume,p=(uint32_t)pan;invoke(buffer_base[15],b->owner,1,&v);invoke(buffer_base[16],b->owner,1,&p);invoke(buffer_base[17],b->owner,1,&rate);
}
static void apply_all(void){for(unsigned i=0;i<256;++i)if(buffers[i].used)apply(&buffers[i]);}
static void finish_b(Buffer*b,uint32_t deferred){if(deferred)b->dirty=1;else{b->current=b->pending;b->dirty=0;apply(b);}}
static void finish_l(uint32_t deferred){if(deferred)listener_dirty=1;else{listener=pending_listener;listener_dirty=0;apply_all();}}
static void init_pending(void){if(!listener_dirty)pending_listener=listener;}
static void init_b(Buffer*b){if(!b->dirty)b->pending=b->current;}
static void ds_query(void){uint32_t self=A32(0),g=A32(1),out=A32(2);if(!out)RET(E_INVALIDARG,3);MEM32(out)=0;if(unknown(g)||exact(g,iid_ds8)||iid(g,0x279afa83)){MEM32(out)=self;++MEM32(self+4);RET(DS_OK,3);}RET(E_NOINTERFACE,3);}
static void proxy_release(void){uint32_t self=A32(0),refs=--MEM32(self+4);if(!refs){uint32_t owner=MEM32(self+8);hle_com_free(self);uint32_t a[1]={owner};hle_call_guest(hle_resolve("DS8Buffer::Release"),1,a);}RET(refs,1);}
static void buf_query(void){uint32_t self=A32(0),g=A32(1),out=A32(2);Buffer*b=meta(self);if(!out)RET(E_INVALIDARG,3);MEM32(out)=0;if(unknown(g)||exact(g,iid_buf8)||iid(g,0x279afa85)){MEM32(out)=self;++MEM32(self+4);RET(DS_OK,3);}uint32_t vt=iid(g,0x279afa84)&&b&&(b->flags&1)?listener_vt:iid(g,0x279afa86)&&b&&(b->flags&0x10)?spatial_vt:0;if(!vt)RET(E_NOINTERFACE,3);uint32_t object=hle_com_new(vt,12);MEM32(object+8)=self;++MEM32(self+4);MEM32(out)=object;RET(DS_OK,3);}
static void proxy_query(void){uint32_t self=A32(0),g=A32(1),out=A32(2),vt=MEM32(self);if(!out)RET(E_INVALIDARG,3);if(unknown(g)||(vt==listener_vt&&iid(g,0x279afa84))||(vt==spatial_vt&&iid(g,0x279afa86))){MEM32(out)=self;++MEM32(self+4);RET(DS_OK,3);}uint32_t args[3]={MEM32(self+8),g,out};RET(hle_call_guest(hle_resolve("DS8Buffer::QueryInterface"),3,args),3);}
static void buf_release(void){uint32_t self=A32(0);Buffer*b=meta(self);if(b&&MEM32(self+4)==1)memset(b,0,sizeof *b);RET(invoke(buffer_base[2],self,0,NULL),1);}
static void buf_set_volume(void){Buffer*b=meta(A32(0));int32_t v=(int32_t)A32(1);if(!b||v<-10000||v>0)RET(E_INVALIDARG,2);b->volume=v;apply(b);RET(DS_OK,2);}
static void buf_get_volume(void){Buffer*b=meta(A32(0));if(!b||!A32(1))RET(E_INVALIDARG,2);MEM32(A32(1))=(uint32_t)b->volume;RET(DS_OK,2);}
static void buf_set_pan(void){Buffer*b=meta(A32(0));int32_t v=(int32_t)A32(1);if(!b||v<-10000||v>10000)RET(E_INVALIDARG,2);b->pan=v;apply(b);RET(DS_OK,2);}
static void buf_set_rate(void){Buffer*b=meta(A32(0));uint32_t v=A32(1);if(!b||(v&&(v<100||v>200000)))RET(E_INVALIDARG,2);b->rate=v?v:b->original_rate;apply(b);RET(DS_OK,2);}
static void buf_set_fx(void){uint32_t count=A32(1),results=A32(3);if(results)for(uint32_t i=0;i<count&&i<64;++i)MEM32(results+i*4)=4;RET(count?E_NOTIMPL:DS_OK,4);}
static void buf_acquire_fx(void){uint32_t count=A32(2),results=A32(3);if(results)for(uint32_t i=0;i<count&&i<64;++i)MEM32(results+i*4)=4;RET(count?E_NOTIMPL:DS_OK,4);}
static void buf_object_path(void){if(A32(4))MEM32(A32(4))=0;RET(E_NOINTERFACE,5);}
static void verify(void){if(!A32(1))RET(E_INVALIDARG,2);MEM32(A32(1))=1;RET(DS_OK,2);}
static Buffer*new_meta(uint32_t object,uint32_t flags,uint32_t rate){Buffer*b=meta(object);if(b)return b;for(unsigned i=0;i<256;++i)if(!buffers[i].used){b=&buffers[i];memset(b,0,sizeof *b);b->used=1;b->owner=object;b->flags=flags;b->rate=b->original_rate=rate?rate:22050;b->current=(B3){64,{0,0,0},{0,0,0},360,360,{0,0,1},0,1,1000000000.f,0};b->pending=b->current;return b;}return NULL;}
static void patch_buffer(uint32_t object,uint32_t flags,uint32_t rate){if(!buffer_vt){uint32_t old=MEM32(object);for(int i=0;i<21;++i)buffer_base[i]=MEM32(old+i*4);uint32_t*vt=hle_alloc(24*4);memcpy(vt,buffer_base,21*4);vt[0]=hle_resolve("DS8Buffer::QueryInterface");vt[2]=hle_resolve("DS8Buffer::Release");vt[6]=hle_resolve("DS8Buffer::GetVolume");vt[15]=hle_resolve("DS8Buffer::SetVolume");vt[16]=hle_resolve("DS8Buffer::SetPan");vt[17]=hle_resolve("DS8Buffer::SetFrequency");vt[21]=hle_resolve("DS8Buffer::SetFX");vt[22]=hle_resolve("DS8Buffer::AcquireResources");vt[23]=hle_resolve("DS8Buffer::GetObjectInPath");buffer_vt=hle_va(vt);}MEM32(object)=buffer_vt;if(!new_meta(object,flags,rate))hle_fatal("DS8 metadata exhausted");}
static void create_buffer(void){uint32_t self=A32(0),desc=A32(1),out=A32(2),args[3]={desc,out,A32(3)};if(!desc||!out)RET(E_INVALIDARG,4);uint32_t flags=MEM32(desc+4),format=MEM32(desc+16),rate=format?MEM32(format+4):22050;uint32_t hr=invoke(device_base[3],self,3,args);if(!hr&&MEM32(out))patch_buffer(MEM32(out),flags,rate);RET(hr,4);}
static void duplicate_buffer(void){uint32_t self=A32(0),source=A32(1),out=A32(2),args[2]={source,out};Buffer*src=meta(source);uint32_t hr=invoke(device_base[5],self,2,args);if(!hr&&out&&MEM32(out)){patch_buffer(MEM32(out),src?src->flags:0,src?src->original_rate:22050);Buffer*b=meta(MEM32(out));if(src){uint32_t owner=b->owner;*b=*src;b->owner=owner;}apply(b);}RET(hr,3);}
static void l_get_all(void){if(!A32(1)||MEM32(A32(1))<64)RET(E_INVALIDARG,2);memcpy(APTR(1),&listener,64);RET(DS_OK,2);}
static void b_get_all(void){Buffer*b=spatial();if(!b||!A32(1)||MEM32(A32(1))<64)RET(E_INVALIDARG,2);memcpy(APTR(1),&b->current,64);RET(DS_OK,2);}
#define L_GET_FLOAT(name,field) static void name(void){if(!A32(1))RET(E_INVALIDARG,2);MEM32(A32(1))=bits(listener.field);RET(DS_OK,2);}
#define L_SET_FLOAT(name,field,allow_zero) static void name(void){float f=flt(A32(1));if(!isfinite(f)||f<0||(!(allow_zero)&&f==0)||A32(2)>1)RET(E_INVALIDARG,3);init_pending();pending_listener.field=f;finish_l(A32(2));RET(DS_OK,3);}
#define L_GET_VEC(name,field) static void name(void){if(!A32(1))RET(E_INVALIDARG,2);memcpy(APTR(1),&listener.field,12);RET(DS_OK,2);}
#define L_SET_VEC(name,field) static void name(void){V3 v={flt(A32(1)),flt(A32(2)),flt(A32(3))};if(!goodvec(v)||A32(4)>1)RET(E_INVALIDARG,5);init_pending();pending_listener.field=v;finish_l(A32(4));RET(DS_OK,5);}
L_GET_FLOAT(l_get_distance,distance) L_GET_FLOAT(l_get_rolloff,rolloff) L_GET_FLOAT(l_get_doppler,doppler)
L_SET_FLOAT(l_set_distance,distance,0) L_SET_FLOAT(l_set_rolloff,rolloff,1) L_SET_FLOAT(l_set_doppler,doppler,1)
L_GET_VEC(l_get_position,position) L_GET_VEC(l_get_velocity,velocity) L_SET_VEC(l_set_position,position) L_SET_VEC(l_set_velocity,velocity)
static int valid_l(const L3*s){return s->size==64&&goodvec(s->position)&&goodvec(s->velocity)&&goodvec(s->front)&&goodvec(s->top)&&length(s->front)>0&&length(s->top)>0&&isfinite(s->distance)&&s->distance>0&&isfinite(s->rolloff)&&s->rolloff>=0&&isfinite(s->doppler)&&s->doppler>=0;}
static void l_set_all(void){if(!A32(1)||A32(2)>1)RET(E_INVALIDARG,3);L3*s=APTR(1);if(!valid_l(s))RET(E_INVALIDARG,3);pending_listener=*s;finish_l(A32(2));RET(DS_OK,3);}
static void l_get_orientation(void){if(!A32(1)||!A32(2))RET(E_INVALIDARG,3);memcpy(APTR(1),&listener.front,12);memcpy(APTR(2),&listener.top,12);RET(DS_OK,3);}
static void l_set_orientation(void){V3 f={flt(A32(1)),flt(A32(2)),flt(A32(3))},t={flt(A32(4)),flt(A32(5)),flt(A32(6))};if(!goodvec(f)||!goodvec(t)||length(f)==0||length(t)==0||A32(7)>1)RET(E_INVALIDARG,8);init_pending();pending_listener.front=f;pending_listener.top=t;finish_l(A32(7));RET(DS_OK,8);}
static void commit(void){if(listener_dirty){listener=pending_listener;listener_dirty=0;}for(unsigned i=0;i<256;++i)if(buffers[i].used&&buffers[i].dirty){buffers[i].current=buffers[i].pending;buffers[i].dirty=0;}apply_all();RET(DS_OK,1);}
#define B_GET_VALUE(name,field) static void name(void){Buffer*b=spatial();if(!b||!A32(1))RET(E_INVALIDARG,2);memcpy(APTR(1),&b->current.field,4);RET(DS_OK,2);}
#define B_GET_VEC(name,field) static void name(void){Buffer*b=spatial();if(!b||!A32(1))RET(E_INVALIDARG,2);memcpy(APTR(1),&b->current.field,12);RET(DS_OK,2);}
#define B_SET_VEC(name,field) static void name(void){Buffer*b=spatial();V3 v={flt(A32(1)),flt(A32(2)),flt(A32(3))};if(!b||!goodvec(v)||A32(4)>1)RET(E_INVALIDARG,5);init_b(b);b->pending.field=v;finish_b(b,A32(4));RET(DS_OK,5);}
B_GET_VALUE(b_get_outside,outside_volume) B_GET_VALUE(b_get_max,max_distance) B_GET_VALUE(b_get_min,min_distance) B_GET_VALUE(b_get_mode,mode)
B_GET_VEC(b_get_cone,cone) B_GET_VEC(b_get_position,position) B_GET_VEC(b_get_velocity,velocity)
B_SET_VEC(b_set_cone,cone) B_SET_VEC(b_set_position,position) B_SET_VEC(b_set_velocity,velocity)
static void b_get_angles(void){Buffer*b=spatial();if(!b||!A32(1)||!A32(2))RET(E_INVALIDARG,3);MEM32(A32(1))=b->current.inside;MEM32(A32(2))=b->current.outside;RET(DS_OK,3);}
static void b_set_angles(void){Buffer*b=spatial();if(!b||A32(1)>A32(2)||A32(2)>360||A32(3)>1)RET(E_INVALIDARG,4);init_b(b);b->pending.inside=A32(1);b->pending.outside=A32(2);finish_b(b,A32(3));RET(DS_OK,4);}
static void b_set_outside(void){Buffer*b=spatial();int32_t v=(int32_t)A32(1);if(!b||v<-10000||v>0||A32(2)>1)RET(E_INVALIDARG,3);init_b(b);b->pending.outside_volume=v;finish_b(b,A32(2));RET(DS_OK,3);}
static void b_set_mode(void){Buffer*b=spatial();if(!b||A32(1)>2||A32(2)>1)RET(E_INVALIDARG,3);init_b(b);b->pending.mode=A32(1);finish_b(b,A32(2));RET(DS_OK,3);}
static void b_set_min(void){Buffer*b=spatial();float v=flt(A32(1));if(!b||!isfinite(v)||v<=0||A32(2)>1)RET(E_INVALIDARG,3);init_b(b);b->pending.min_distance=v;finish_b(b,A32(2));RET(DS_OK,3);}
static void b_set_max(void){Buffer*b=spatial();float v=flt(A32(1));if(!b||!isfinite(v)||v<=0||A32(2)>1)RET(E_INVALIDARG,3);init_b(b);b->pending.max_distance=v;finish_b(b,A32(2));RET(DS_OK,3);}
static void b_set_all(void){Buffer*b=spatial();B3*s=APTR(1);if(!b||!s||s->size!=64||!goodvec(s->position)||!goodvec(s->velocity)||!goodvec(s->cone)||s->inside>s->outside||s->outside>360||s->outside_volume<-10000||s->outside_volume>0||!isfinite(s->min_distance)||!isfinite(s->max_distance)||s->min_distance<=0||s->max_distance<=0||s->mode>2||A32(2)>1)RET(E_INVALIDARG,3);b->pending=*s;finish_b(b,A32(2));RET(DS_OK,3);}
static void vtables(void){if(listener_vt)return;static const win32hle_shim l[]={
{"DS3DListener::QueryInterface",proxy_query},{"DS3DListener::AddRef",hle_com_AddRef},{"DS3DListener::Release",proxy_release},{"DS3DListener::GetAllParameters",l_get_all},{"DS3DListener::GetDistanceFactor",l_get_distance},{"DS3DListener::GetDopplerFactor",l_get_doppler},{"DS3DListener::GetOrientation",l_get_orientation},{"DS3DListener::GetPosition",l_get_position},{"DS3DListener::GetRolloffFactor",l_get_rolloff},{"DS3DListener::GetVelocity",l_get_velocity},{"DS3DListener::SetAllParameters",l_set_all},{"DS3DListener::SetDistanceFactor",l_set_distance},{"DS3DListener::SetDopplerFactor",l_set_doppler},{"DS3DListener::SetOrientation",l_set_orientation},{"DS3DListener::SetPosition",l_set_position},{"DS3DListener::SetRolloffFactor",l_set_rolloff},{"DS3DListener::SetVelocity",l_set_velocity},{"DS3DListener::CommitDeferredSettings",commit},{0,0}};
static const win32hle_shim b[]={
{"DS3DBuffer::QueryInterface",proxy_query},{"DS3DBuffer::AddRef",hle_com_AddRef},{"DS3DBuffer::Release",proxy_release},{"DS3DBuffer::GetAllParameters",b_get_all},{"DS3DBuffer::GetConeAngles",b_get_angles},{"DS3DBuffer::GetConeOrientation",b_get_cone},{"DS3DBuffer::GetConeOutsideVolume",b_get_outside},{"DS3DBuffer::GetMaxDistance",b_get_max},{"DS3DBuffer::GetMinDistance",b_get_min},{"DS3DBuffer::GetMode",b_get_mode},{"DS3DBuffer::GetPosition",b_get_position},{"DS3DBuffer::GetVelocity",b_get_velocity},{"DS3DBuffer::SetAllParameters",b_set_all},{"DS3DBuffer::SetConeAngles",b_set_angles},{"DS3DBuffer::SetConeOrientation",b_set_cone},{"DS3DBuffer::SetConeOutsideVolume",b_set_outside},{"DS3DBuffer::SetMaxDistance",b_set_max},{"DS3DBuffer::SetMinDistance",b_set_min},{"DS3DBuffer::SetMode",b_set_mode},{"DS3DBuffer::SetPosition",b_set_position},{"DS3DBuffer::SetVelocity",b_set_velocity},{0,0}};listener_vt=hle_com_vtable(l);spatial_vt=hle_com_vtable(b);}
static void direct_sound_create8(void){uint32_t out=A32(1),args[3]={A32(0),out,A32(2)},target=hle_resolve("DirectSoundCreate");if(!out||A32(2))RET(E_INVALIDARG,3);if(!target){MEM32(out)=0;RET(E_NOTIMPL,3);}uint32_t hr=hle_call_guest(target,3,args);if(!hr&&MEM32(out)){vtables();uint32_t object=MEM32(out);if(!device_vt){uint32_t old=MEM32(object);for(int i=0;i<11;++i)device_base[i]=MEM32(old+i*4);uint32_t*vt=hle_alloc(12*4);memcpy(vt,device_base,11*4);vt[0]=hle_resolve("DS8::QueryInterface");vt[3]=hle_resolve("DS8::CreateSoundBuffer");vt[5]=hle_resolve("DS8::DuplicateSoundBuffer");vt[11]=hle_resolve("DS8::VerifyCertification");device_vt=hle_va(vt);}MEM32(object)=device_vt;}RET(hr,3);}
const win32hle_shim ds_dsound8_shims[]={
{"DirectSoundCreate8",direct_sound_create8},{"DS8::QueryInterface",ds_query},{"DS8::CreateSoundBuffer",create_buffer},{"DS8::DuplicateSoundBuffer",duplicate_buffer},{"DS8::VerifyCertification",verify},
{"DS8Buffer::QueryInterface",buf_query},{"DS8Buffer::Release",buf_release},{"DS8Buffer::GetVolume",buf_get_volume},{"DS8Buffer::SetVolume",buf_set_volume},{"DS8Buffer::SetPan",buf_set_pan},{"DS8Buffer::SetFrequency",buf_set_rate},{"DS8Buffer::SetFX",buf_set_fx},{"DS8Buffer::AcquireResources",buf_acquire_fx},{"DS8Buffer::GetObjectInPath",buf_object_path},{0,0}};
