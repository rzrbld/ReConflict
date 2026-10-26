#include "d3d8_lights.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
unsigned baseline_d8_lights_pack(const D8LightBank *,float [8][26],uint32_t [8],uint32_t [8]);
static uint32_t rng=0x78b1d04a;
static uint32_t next(void){rng=rng*1664525u+1013904223u;return rng;}
static int compare(const D8LightBank *bank,unsigned trial){
 float a[8][26],b[8][26];uint32_t e[8],f[8],i[8],j[8];
 memset(a,0xa5,sizeof a);memset(b,0x5a,sizeof b);memset(e,0xa5,sizeof e);memset(f,0x5a,sizeof f);memset(i,0xa5,sizeof i);memset(j,0x5a,sizeof j);
 unsigned n=baseline_d8_lights_pack(bank,a,e,i),m=ds_d8_lights_pack(bank,b,f,j);
 if(n!=m||memcmp(a,b,sizeof a)||memcmp(e,f,sizeof e)||memcmp(i,j,sizeof i)){printf("FAIL trial=%u active=%u\n",trial,bank?bank->active_count:0);return 1;}
 memset(a,0xa5,sizeof a);memset(b,0x5a,sizeof b);
 n=baseline_d8_lights_pack(bank,a,e,NULL);m=ds_d8_lights_pack(bank,b,f,NULL);
 return n!=m||memcmp(a,b,sizeof a)||memcmp(e,f,sizeof e);
}
int main(void){
 D8LightBank bank;ds_d8_lights_init(&bank);
 if(compare(NULL,0)||compare(&bank,1))return 1;
 unsigned ids[8]={0,1,3,8,90,200,253,255};
 for(unsigned n=0;n<8;n++){if(ds_d8_lights_enable(&bank,ids[n],1)||compare(&bank,100001+n))return 1;}
 for(unsigned n=0;n<8;n++){if(ds_d8_lights_enable(&bank,ids[n],0)||compare(&bank,100010+n))return 1;}
 for(unsigned n=0;n<100000;n++){
  uint32_t r=next(),id=r>>24;
  if((r>>16)&1){float l[26];for(unsigned k=0;k<26;k++)l[k]=(int32_t)next()/2147483648.f;uint32_t t=next()%3+1;memcpy(l,&t,4);ds_d8_lights_set(&bank,id,l);}
  else ds_d8_lights_enable(&bank,id,(r>>8)&1);
  if(!(n%17)){
   D8LightBank source=bank,before=bank;D8LightMask mask;ds_d8_light_mask_clear(&mask);
   for(unsigned k=0;k<8;k++){
    uint32_t v=next(),slot=v>>24;
    ds_d8_lights_enable(&source,slot,(v>>9)&1);
    mask.definition[slot]=(v>>12)&1;mask.enable[slot]=(v>>13)&1;
   }
   int rc=ds_d8_lights_apply_masked(&bank,&source,&mask);
   if(rc&&memcmp(&before,&bank,sizeof bank)){puts("FAIL: rejected masked operation changed bank");return 1;}
  }
  if(compare(&bank,n+2))return 1;
 }
 puts("100000 set/enable/masked-state operations +0..8 active/high IDs + NULL/empty/optional IDs bit-exact PASS");return 0;
}
