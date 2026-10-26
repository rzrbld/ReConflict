/* Standalone controller-operated setup. Called before guest/SDL game boot. */
#include "configuration.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>
/* Original 5x7 uppercase bitmap font; no font or SDL_ttf runtime dependency. */
static const unsigned char glyph[36][7] = {
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,14},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,27,17},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14}};

static void text(SDL_Renderer *r, int x, int y, int scale, const char *s) {
    static const unsigned char percent[7] = {24,25,2,4,8,19,3};
    SDL_Rect pixels[4096]; int count = 0;
    for (; *s; ++s, x += 6 * scale) {
        int index = *s >= 'A' && *s <= 'Z' ? *s - 'A' : *s >= '0' && *s <= '9' ? 26 + *s - '0' : -1;
        for (int row = 0; row < 7; ++row) {
            unsigned bits = index >= 0 ? glyph[index][row] :
                            *s == '-' && row == 3 ? 31 : *s == '.' && row == 6 ? 4 :
                            *s == ':' && (row == 2 || row == 5) ? 4 :
                            *s == '%' ? percent[row] : 0;
            for (int col = 0; col < 5 && count < 4096; ++col)
                if (bits & (1u << (4 - col))) pixels[count++] = (SDL_Rect){x + col * scale, y + row * scale, scale, scale};
        }
    }
    SDL_RenderFillRects(r, pixels, count);
}
static void color(SDL_Renderer *r, int red, int green, int blue) { SDL_SetRenderDrawColor(r, red, green, blue, 255); }


static SDL_GameController *open_pad(void){
 for(int i=0;i<SDL_NumJoysticks();i++)if(SDL_IsGameController(i)){
  SDL_GameController *pad=SDL_GameControllerOpen(i);if(pad)return pad;
 }return NULL;
}
static void cycle(DSGraphicsConfig *s,int i,int direction){
 if(!(s->present&(1u<<i))){s->present|=1u<<i;s->value[i]=direction<0?1:0;}
 else if((direction>0&&s->value[i]==1)||(direction<0&&s->value[i]==0))s->present&=~(1u<<i);
 else s->value[i]=!s->value[i];
}
int ds_configuration_main(void){
 DSGraphicsConfig s;ds_configuration_defaults(&s);
 int loaded=ds_configuration_load(&s);
 if(loaded<0)return 1; /* Do not overwrite a malformed user configuration. */
 int indices[DS_GRAPHICS_COUNT],count=0;
 for(int i=0;i<DS_GRAPHICS_COUNT;i++)if(ds_graphics_options[i].editable)indices[count++]=i;
 SDL_SetHint(SDL_HINT_RENDER_DRIVER,"opengles2");
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)){fprintf(stderr,"setup SDL: %s\n",SDL_GetError());return 1;}
 SDL_Window *w=SDL_CreateWindow("Desert Storm Graphics Setup",0,0,640,480,SDL_WINDOW_FULLSCREEN_DESKTOP);
 SDL_Renderer *r=w?SDL_CreateRenderer(w,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC):NULL;
 if(!r&&w)r=SDL_CreateRenderer(w,-1,SDL_RENDERER_SOFTWARE);
 if(!r){fprintf(stderr,"setup window: %s\n",SDL_GetError());if(w)SDL_DestroyWindow(w);SDL_Quit();return 1;}
 SDL_RenderSetLogicalSize(r,640,480);
 SDL_GameController *pad=open_pad();int selected=0,done=0,result=0;
 Uint32 repeat=0;const char *message="KEEP PRESERVES EXISTING SETTINGS";
 while(!done){
  SDL_Event e;int move=0,adjust=0,accept=0;
  while(SDL_PollEvent(&e)){
   if(e.type==SDL_QUIT)done=1;
   if(e.type==SDL_CONTROLLERDEVICEADDED&&!pad)pad=open_pad();
   if(e.type==SDL_CONTROLLERDEVICEREMOVED&&pad&&!SDL_GameControllerGetAttached(pad)){SDL_GameControllerClose(pad);pad=open_pad();}
   if(e.type==SDL_CONTROLLERBUTTONDOWN){
    switch(e.cbutton.button){
     case SDL_CONTROLLER_BUTTON_DPAD_UP:move=-1;break;
     case SDL_CONTROLLER_BUTTON_DPAD_DOWN:move=1;break;
     case SDL_CONTROLLER_BUTTON_DPAD_LEFT:adjust=-1;break;
     case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:adjust=1;break;
     case SDL_CONTROLLER_BUTTON_A:accept=1;break;
     case SDL_CONTROLLER_BUTTON_B:done=1;break;
    }
   }
   if(e.type==SDL_KEYDOWN){switch(e.key.keysym.sym){
    case SDLK_UP:move=-1;break;case SDLK_DOWN:move=1;break;
    case SDLK_LEFT:adjust=-1;break;case SDLK_RIGHT:adjust=1;break;
    case SDLK_RETURN:accept=1;break;case SDLK_ESCAPE:done=1;break;
   }}
  }
  if(pad&&SDL_GetTicks()>=repeat){
   Sint16 x=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTX),y=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTY);
   if(y < -16000)move=-1;else if(y>16000)move=1;else if(x < -16000)adjust=-1;else if(x>16000)adjust=1;
   if(move||adjust)repeat=SDL_GetTicks()+180;
  }
  selected=(selected+move+count+2)%(count+2);
  if(adjust&&selected<count)cycle(&s,indices[selected],adjust);
  if(accept){
   if(selected<count)cycle(&s,indices[selected],1);
   else if(selected==count){if(!ds_configuration_save(&s)){done=1;result=0;}else{message="SAVE FAILED - TRY AGAIN";result=1;}}
   else done=1;
  }
  color(r,21,25,25);SDL_RenderClear(r);
  color(r,226,191,119);text(r,24,20,3,"DESERT STORM SETUP");
  color(r,151,163,157);text(r,24,54,2,"640 X 480 - OPENGL ES");
  for(int row=0;row<count+2;row++){
   int y=94+row*39;
   if(row==selected){color(r,54,70,57);SDL_Rect box={16,y-8,608,34};SDL_RenderFillRect(r,&box);}
   color(r,row==selected?245:185,row==selected?220:192,row==selected?150:178);
   if(row<count){int i=indices[row];text(r,28,y,2,ds_graphics_options[i].label);char val[20];
    if(s.present&(1u<<i))snprintf(val,sizeof val,"%u",s.value[i]);else snprintf(val,sizeof val,"KEEP");
    text(r,510,y,2,val);
   }else text(r,28,y,2,row==count?"SAVE AND EXIT":"CANCEL");
  }
  color(r,151,163,157);text(r,24,400,2,message);
  text(r,24,430,2,"DPAD OR STICK - CHANGE");text(r,24,454,2,"A SELECT   B CANCEL");
  SDL_RenderPresent(r);SDL_Delay(8);
 }
 if(pad)SDL_GameControllerClose(pad);
 SDL_DestroyRenderer(r);SDL_DestroyWindow(w);SDL_Quit();return result;
}
