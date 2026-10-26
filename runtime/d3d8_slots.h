#ifndef DS_D3D8_SLOTS_H
#define DS_D3D8_SLOTS_H
#include <stdint.h>
/* Slot zero is invalid. Metadata is zero-initialized and pages are touched
 * only as needed. Both fresh allocation and recycling take constant time. */
#define DS_D3D8_SLOT_CAPACITY 65536u
typedef struct { uint32_t next,free_head,link[DS_D3D8_SLOT_CAPACITY]; } D8Slots;
static inline uint32_t ds_d8_slot_take(D8Slots *p){
    uint32_t id;
    if(p->free_head){id=p->free_head;p->free_head=p->link[id];}
    else{if(!p->next)p->next=1;if(p->next>=DS_D3D8_SLOT_CAPACITY)return 0;id=p->next++;}
    p->link[id]=UINT32_MAX;return id;
}
static inline int ds_d8_slot_put(D8Slots *p,uint32_t id){
    if(!id||id>=p->next||id>=DS_D3D8_SLOT_CAPACITY||p->link[id]!=UINT32_MAX)return -1;
    p->link[id]=p->free_head;p->free_head=id;return 0;
}
#endif
