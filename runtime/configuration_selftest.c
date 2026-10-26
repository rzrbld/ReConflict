#define _POSIX_C_SOURCE 200809L
#include "configuration.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int input(void *unused){
 (void)unused;SDL_Delay(300);
 SDL_Event e;memset(&e,0,sizeof e);e.type=SDL_CONTROLLERBUTTONDOWN;
 e.cbutton.button=SDL_CONTROLLER_BUTTON_DPAD_RIGHT;assert(SDL_PushEvent(&e)==1);
 for(int i=0;i<5;i++){SDL_Delay(100);e.cbutton.button=SDL_CONTROLLER_BUTTON_DPAD_DOWN;assert(SDL_PushEvent(&e)==1);}
 SDL_Delay(100);e.cbutton.button=SDL_CONTROLLER_BUTTON_A;assert(SDL_PushEvent(&e)==1);return 0;
}
int main(void){
 char root[]="/tmp/ds-configuration-XXXXXX";assert(mkdtemp(root));setenv("DS_ROOT",root,1);
 char dir[256],path[256];snprintf(dir,sizeof dir,"%s/config",root);assert(!mkdir(dir,0700));
 snprintf(path,sizeof path,"%s/config/graphics.cfg",root);
 DSGraphicsConfig s;ds_configuration_defaults(&s);assert(s.value[6]==1&&s.value[7]==1&&s.value[9]==0);assert(ds_configuration_load(&s)==0&&s.present==0);
 FILE *f=fopen(path,"w");assert(f);fputs("  Decals = 0 \nTextureResolution=1\n",f);fclose(f);
 assert(ds_configuration_load(&s)==1);assert(s.present==((1u<<6)|(1u<<9)));assert(s.value[9]==0);
 DSGraphicsConfig before=s;
 f=fopen(path,"w");assert(f);fputs("Decals=1\nResolutionIndex=9\n",f);fclose(f);
 assert(ds_configuration_load(&s)==-1);assert(!memcmp(&s,&before,sizeof s));
 f=fopen(path,"w");assert(f);fputs("Unknown=1\n",f);fclose(f);assert(ds_configuration_load(&s)==-1);
 s.present=0;assert(!ds_configuration_save(&s));
 setenv("SDL_VIDEODRIVER","dummy",1);setenv("SDL_RENDER_DRIVER","software",1);
 assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER));
 SDL_Thread *t=SDL_CreateThread(input,"config-test",NULL);assert(t);
 assert(!ds_configuration_main());SDL_WaitThread(t,NULL);
 ds_configuration_defaults(&s);assert(ds_configuration_load(&s)==1);
 assert(s.present==(1u<<2)&&s.value[2]==0);
 printf("PASS: missing file; selective key mapping; invalid range/unknown key atomic rejection; atomic save; SDL controller navigation/save preserves omitted keys\n");
 unlink(path);rmdir(dir);rmdir(root);return 0;
}
