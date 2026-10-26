#include "d3d8_slots.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static D8Slots slots;
static unsigned char seen[DS_D3D8_SLOT_CAPACITY];
int main(void){
 for(unsigned i=1;i<DS_D3D8_SLOT_CAPACITY;i++){uint32_t id=ds_d8_slot_take(&slots);assert(id&&!seen[id]);seen[id]=1;}
 assert(!ds_d8_slot_take(&slots));assert(ds_d8_slot_put(&slots,0)==-1);assert(ds_d8_slot_put(&slots,DS_D3D8_SLOT_CAPACITY)==-1);
 for(unsigned i=2;i<DS_D3D8_SLOT_CAPACITY;i+=2){assert(!ds_d8_slot_put(&slots,i));assert(ds_d8_slot_put(&slots,i)==-1);seen[i]=0;}
 for(unsigned i=2;i<DS_D3D8_SLOT_CAPACITY;i+=2){uint32_t id=ds_d8_slot_take(&slots);assert(id&&!seen[id]);seen[id]=1;}
 assert(!ds_d8_slot_take(&slots));
 for(unsigned cycle=0;cycle<100000;cycle++){unsigned id=1+cycle%(DS_D3D8_SLOT_CAPACITY-1);assert(!ds_d8_slot_put(&slots,id));assert(ds_d8_slot_take(&slots)==id);}
 for(unsigned i=1;i<DS_D3D8_SLOT_CAPACITY;i++)assert(!ds_d8_slot_put(&slots,i));
 memset(seen,0,sizeof seen);
 for(unsigned i=1;i<DS_D3D8_SLOT_CAPACITY;i++){uint32_t id=ds_d8_slot_take(&slots);assert(id&&!seen[id]);seen[id]=1;}
 assert(!ds_d8_slot_take(&slots));puts("PASS: 65535 simultaneous unique slots, exhaustion, invalid/double return, alternating reuse,100000 recycle cycles and complete reuse");
}
