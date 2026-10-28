#ifndef DS_PAD_EDGES_H
#define DS_PAD_EDGES_H
#include "../probe/desert_pad.h"
#include <string.h>
/* Preserve transitions across event pumps; only a guest state read consumes one.
 * Bounded per-button queues prevent an unbounded backlog after a long stall. */
typedef struct { unsigned char states[8],head,count,observed; } DS_ButtonEdges;
typedef struct { DS_ButtonEdges button[16]; } DS_PadEdges;
static inline void ds_edges_reset(DS_PadEdges *q){memset(q,0,sizeof *q);}
static inline void ds_edges_push(DS_PadEdges *q,int button,int down){
 DS_ButtonEdges *b=&q->button[button];down=!!down;
 if(b->observed==down)return;
 b->observed=(unsigned char)down;
 if(b->count==8){b->head=(b->head+1)%8;--b->count;}
 b->states[(b->head+b->count)%8]=(unsigned char)down;++b->count;
}
static inline void ds_edges_event(DS_PadEdges *q,const DS_PadConfig *c,const SDL_Event *e){
 for(int i=0;i<16;i++){
  int binding=c->button[i];
  if((e->type==SDL_CONTROLLERBUTTONDOWN||e->type==SDL_CONTROLLERBUTTONUP)&&binding==e->cbutton.button)
   ds_edges_push(q,i,e->type==SDL_CONTROLLERBUTTONDOWN);
  else if(e->type==SDL_CONTROLLERAXISMOTION &&
    ((binding==100&&e->caxis.axis==SDL_CONTROLLER_AXIS_TRIGGERLEFT)||
     (binding==101&&e->caxis.axis==SDL_CONTROLLER_AXIS_TRIGGERRIGHT)))
   ds_edges_push(q,i,e->caxis.value>32767*c->trigger_threshold);
 }
}
static inline void ds_edges_sample(DS_PadEdges *q,DS_PadState *state){
 for(int i=0;i<16;i++){
  ds_edges_push(q,i,state->buttons[i]!=0);
  DS_ButtonEdges *b=&q->button[i];
  if(b->count){state->buttons[i]=b->states[b->head]?0x80:0;b->head=(b->head+1)%8;--b->count;}
 }
}
#endif
