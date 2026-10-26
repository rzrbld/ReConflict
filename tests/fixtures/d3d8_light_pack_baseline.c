/* Literal pre-optimization packer retained as differential reference. */
#include "d3d8_lights.h"
#include <string.h>
unsigned baseline_d8_lights_pack(const D8LightBank *bank,float light[8][26],uint32_t enabled[8],uint32_t ids[8]){
    memset(light,0,8*26*sizeof(float));memset(enabled,0,8*sizeof(uint32_t));
    if(ids)for(unsigned i=0;i<8;i++)ids[i]=UINT32_MAX;
    unsigned n=0;
    if(bank)for(unsigned i=0;i<DS_D8_LOGICAL_LIGHTS&&n<8;i++)if(bank->defined[i]&&bank->enabled[i]){
        memcpy(light[n],bank->light[i],26*sizeof(float));enabled[n]=1;if(ids)ids[n]=i;n++;
    }
    return n;
}
