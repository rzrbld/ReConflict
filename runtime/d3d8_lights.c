#include "d3d8_lights.h"
#include <string.h>
static uint32_t light_type(const float light[26]){uint32_t type;memcpy(&type,light,4);return type;}
static int valid_type(const float light[26]){uint32_t t=light_type(light);return t>=1&&t<=3;}
static void default_light(float light[26]){
    /* Microsoft IDirect3DDevice8::LightEnable defaults: directional,
     * diffuse(1,1,1,0), direction(0,0,1), every other component zero. */
    memset(light,0,26*sizeof(float));uint32_t type=3;memcpy(light,&type,4);
    light[1]=light[2]=light[3]=1.f;light[18]=1.f;
}
void ds_d8_lights_init(D8LightBank *bank){if(bank)memset(bank,0,sizeof *bank);}
int ds_d8_lights_set(D8LightBank *bank,uint32_t id,const float light[26]){
    if(!bank||!light||id>=DS_D8_LOGICAL_LIGHTS||!valid_type(light))return -1;
    /* memmove also permits passing this bank's own definition as input. */
    memmove(bank->light[id],light,26*sizeof(float));bank->defined[id]=1;return 0;
}
int ds_d8_lights_get(const D8LightBank *bank,uint32_t id,float light[26]){
    if(!bank||!light||id>=DS_D8_LOGICAL_LIGHTS||!bank->defined[id])return -1;
    memmove(light,bank->light[id],26*sizeof(float));return 0;
}
int ds_d8_lights_enable(D8LightBank *bank,uint32_t id,int enable){
    if(!bank||id>=DS_D8_LOGICAL_LIGHTS)return -1;
    uint8_t value=enable!=0;
    if(value&&!bank->enabled[id]&&bank->active_count>=DS_D8_ACTIVE_LIGHTS)return -1;
    if(!bank->defined[id]){default_light(bank->light[id]);bank->defined[id]=1;}
    if(value!=bank->enabled[id]){if(value)bank->active_count++;else bank->active_count--;bank->enabled[id]=value;}
    return 0;
}
int ds_d8_lights_get_enable(const D8LightBank *bank,uint32_t id,uint32_t *enable){
    if(!bank||!enable||id>=DS_D8_LOGICAL_LIGHTS||!bank->defined[id])return -1;
    *enable=bank->enabled[id]!=0;return 0;
}
unsigned ds_d8_lights_pack(const D8LightBank *bank,float light[8][26],uint32_t enabled[8],uint32_t ids[8]){
    unsigned n=0;
    /* Bank mutation helpers keep active_count exact, including masked restores. */
    if(bank)for(unsigned i=0;i<DS_D8_LOGICAL_LIGHTS&&n<8&&n<bank->active_count;i++)if(bank->defined[i]&&bank->enabled[i]){
        memcpy(light[n],bank->light[i],26*sizeof(float));enabled[n]=1;if(ids)ids[n]=i;n++;
    }
    /* Avoid zeroing slots immediately overwritten with active definitions. */
    memset(light+n,0,(8-n)*26*sizeof(float));memset(enabled+n,0,(8-n)*sizeof(uint32_t));
    if(ids)for(unsigned i=n;i<8;i++)ids[i]=UINT32_MAX;
    return n;
}
void ds_d8_light_mask_clear(D8LightMask *mask){if(mask)memset(mask,0,sizeof *mask);}
void ds_d8_light_mask_all(D8LightMask *mask){if(mask)memset(mask,1,sizeof *mask);}
int ds_d8_lights_apply_masked(D8LightBank *bank,const D8LightBank *source,const D8LightMask *mask){
    if(!bank||!source||!mask)return -1;
    D8LightBank candidate=*bank;unsigned active=0;
    for(unsigned i=0;i<DS_D8_LOGICAL_LIGHTS;i++){
        if(mask->definition[i]){
            candidate.defined[i]=source->defined[i]!=0;
            if(candidate.defined[i])memcpy(candidate.light[i],source->light[i],26*sizeof(float));
            else memset(candidate.light[i],0,26*sizeof(float));
        }
        if(mask->enable[i]){
            candidate.enabled[i]=source->enabled[i]!=0;
            /* Enable-only API records on an undefined current light create
             * the default whether enabling OR disabling. A wholly undefined
             * captured slot remains absent during complete snapshot restore. */
            if(!candidate.defined[i]&&source->defined[i]&&!mask->definition[i]){
                default_light(candidate.light[i]);candidate.defined[i]=1;
            }
        }
        if(candidate.enabled[i]&&!candidate.defined[i])return -1;
        if(candidate.defined[i]&&!valid_type(candidate.light[i]))return -1;
        active+=candidate.enabled[i]!=0;
    }
    if(active>DS_D8_ACTIVE_LIGHTS)return -1;
    candidate.active_count=active;*bank=candidate;return 0;
}
