#include "d3d8_lights.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t type(const float *light){uint32_t t;memcpy(&t,light,4);return t;}
static void definition(float *light,uint32_t kind,float seed){
    for(unsigned i=0;i<26;i++)light[i]=seed+(float)i/32.f;
    memcpy(light,&kind,4);
}
static void defaults(const float *light){
    float expected[26]={0};uint32_t kind=3;memcpy(expected,&kind,4);
    expected[1]=expected[2]=expected[3]=expected[18]=1.f;
    assert(!memcmp(light,expected,sizeof expected));
}
int main(void){
    D8LightBank bank,before,source;D8LightMask mask;
    float light[26],out[26],packed[8][26];uint32_t enabled[8],ids[8],value;
    ds_d8_lights_init(&bank);memset(out,0xa5,sizeof out);memcpy(light,out,sizeof out);value=0xdeadbeef;
    assert(ds_d8_lights_get(&bank,253,out)==-1&&!memcmp(out,light,sizeof out));
    assert(ds_d8_lights_get_enable(&bank,253,&value)==-1&&value==0xdeadbeef);
    definition(light,1,.25f);assert(!ds_d8_lights_set(&bank,253,light));
    assert(!ds_d8_lights_get(&bank,253,out)&&!memcmp(out,light,sizeof out));
    assert(!ds_d8_lights_get_enable(&bank,253,&value)&&value==0);
    assert(!ds_d8_lights_enable(&bank,253,1));
    definition(light,2,.5f);assert(!ds_d8_lights_set(&bank,8,light));assert(!ds_d8_lights_enable(&bank,8,1));
    assert(!ds_d8_lights_enable(&bank,0,1));defaults(bank.light[0]);
    assert(ds_d8_lights_pack(&bank,packed,enabled,ids)==3);
    assert(ids[0]==0&&ids[1]==8&&ids[2]==253&&ids[3]==UINT32_MAX);
    assert(type(packed[0])==3&&type(packed[1])==2&&type(packed[2])==1);
    for(unsigned i=3;i<8;i++){float zero[26]={0};assert(!enabled[i]&&!memcmp(packed[i],zero,sizeof zero));}
    assert(!ds_d8_lights_enable(&bank,8,0));assert(ds_d8_lights_pack(&bank,packed,enabled,ids)==2&&ids[1]==253);
    assert(!ds_d8_lights_enable(&bank,8,1));
    for(unsigned i=1;i<=5;i++)assert(!ds_d8_lights_enable(&bank,i,1));
    assert(bank.active_count==8);before=bank;
    assert(ds_d8_lights_enable(&bank,200,1)==-1&&!memcmp(&bank,&before,sizeof bank));
    assert(!ds_d8_lights_enable(&bank,253,17)&&!memcmp(&bank,&before,sizeof bank));
    assert(!ds_d8_lights_enable(&bank,1,0));assert(!ds_d8_lights_enable(&bank,200,1));
    assert(ds_d8_lights_pack(&bank,packed,enabled,ids)==8&&ids[6]==200&&ids[7]==253);
    definition(light,1,.75f);assert(!ds_d8_lights_set(&bank,253,light));assert(bank.enabled[253]&&bank.active_count==8);
    assert(!memcmp(bank.light[253],light,sizeof light));
    before=bank;memset(light,0,sizeof light);
    assert(ds_d8_lights_set(&bank,256,light)==-1);
    assert(ds_d8_lights_set(&bank,4,light)==-1);
    assert(ds_d8_lights_enable(&bank,256,0)==-1);
    assert(ds_d8_lights_get(&bank,256,out)==-1);
    assert(ds_d8_lights_get_enable(&bank,256,&value)==-1);
    assert(!memcmp(&bank,&before,sizeof bank));
    assert(!ds_d8_lights_enable(&bank,254,0));assert(bank.defined[254]&&!bank.enabled[254]);defaults(bank.light[254]);
    /* Separate definition and enable masks, as recorded by original API calls. */
    ds_d8_lights_init(&bank);ds_d8_lights_init(&source);ds_d8_light_mask_clear(&mask);
    definition(light,1,.125f);assert(!ds_d8_lights_set(&source,253,light));assert(!ds_d8_lights_enable(&source,253,1));
    mask.enable[253]=1;assert(!ds_d8_lights_apply_masked(&bank,&source,&mask));
    assert(bank.enabled[253]&&bank.active_count==1);defaults(bank.light[253]);
    ds_d8_light_mask_clear(&mask);mask.definition[253]=1;
    assert(!ds_d8_lights_apply_masked(&bank,&source,&mask));assert(!memcmp(bank.light[253],light,sizeof light));assert(bank.enabled[253]);
    definition(bank.light[253],2,.375f);memcpy(out,bank.light[253],sizeof out);
    ds_d8_light_mask_clear(&mask);mask.enable[253]=1;
    assert(!ds_d8_lights_apply_masked(&bank,&source,&mask));assert(!memcmp(bank.light[253],out,sizeof out));
    /* Ninth-light masked restore is transactional too. */
    ds_d8_lights_init(&bank);for(unsigned i=0;i<8;i++)assert(!ds_d8_lights_enable(&bank,i,1));
    before=bank;assert(ds_d8_lights_apply_masked(&bank,&source,&mask)==-1&&!memcmp(&bank,&before,sizeof bank));
    /* Apply disables+enables together; final8 is valid despite old8 being full. */
    mask.enable[0]=1;assert(!ds_d8_lights_apply_masked(&bank,&source,&mask));
    assert(!bank.enabled[0]&&bank.enabled[253]&&bank.active_count==8);defaults(bank.light[253]);
    /* Definition-only removal of an active light must not create invalid state. */
    D8LightBank empty;ds_d8_lights_init(&empty);ds_d8_light_mask_clear(&mask);mask.definition[253]=1;
    before=bank;assert(ds_d8_lights_apply_masked(&bank,&empty,&mask)==-1&&!memcmp(&bank,&before,sizeof bank));
    ds_d8_light_mask_all(&mask);assert(!ds_d8_lights_apply_masked(&bank,&source,&mask));assert(!memcmp(&bank,&source,sizeof bank));
    assert(!ds_d8_lights_apply_masked(&bank,&empty,&mask));assert(!memcmp(&bank,&empty,sizeof bank));
    assert(ds_d8_lights_pack(&bank,packed,enabled,NULL)==0);
    /* A disabled undefined light created by Enable(false) restores its default. */
    assert(!ds_d8_lights_enable(&source,255,0));ds_d8_light_mask_clear(&mask);mask.enable[255]=1;
    assert(!ds_d8_lights_apply_masked(&bank,&source,&mask));assert(bank.defined[255]&&!bank.enabled[255]);defaults(bank.light[255]);
    puts("PASS: IDs0/8/200/253/254/255, exact104-byte descriptors, documented undefined defaults, ascending8-slot packing/unused clearing, disable/re-enable, ninth-light no-mutation, separate state masks, transactional swaps/full restore");
    return 0;
}
