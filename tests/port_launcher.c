/* Drive the real SDL loop with deterministic controller events and a fake
 * importer. Game data and the user's installation are never touched. */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <SDL.h>
static int scripted_poll(SDL_Event *e);
#define SDL_PollEvent scripted_poll
#define main launcher_main
#include "../probe/port_launcher.c"
#undef main
#undef SDL_PollEvent
#include <assert.h>

static int step,scenario,virtual_index=-1;
static Uint32 next_action,started;
static int scripted_poll(SDL_Event *e){
 if(SDL_PollEvent(e))return 1;
 Uint32 now=SDL_GetTicks();assert(now-started<10000);
 if(virtual_index<0){
  virtual_index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,SDL_CONTROLLER_BUTTON_MAX,0);
  assert(virtual_index>=0);scan_pad();assert(pad&&pad_id>=0);
  SDL_Window*w=SDL_GetWindowFromID(1);assert(w);
  assert(SDL_GetWindowFlags(w)&SDL_WINDOW_RESIZABLE);
  SDL_SetWindowSize(w,960,540);int x,y;SDL_GetWindowSize(w,&x,&y);assert(x==960&&y==540);
 }
 if(now<next_action)return 0;
 int key=-1;
 if(scenario==0){ /* Keep -> FPS off -> Save and Run; reconnect remains usable. */
  if(step==0&&page==DELETE_PROMPT){assert(selected==0);key=SDL_CONTROLLER_BUTTON_A;}
  else if((step==1||step==2)&&page==OPTIONS)key=SDL_CONTROLLER_BUTTON_DPAD_UP;
  else if(step==3&&selected==graphics_count)key=SDL_CONTROLLER_BUTTON_A;
  else if(step==4&&fps==0){
   SDL_JoystickDetachVirtual(virtual_index);scan_pad();assert(!pad);
   virtual_index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,SDL_CONTROLLER_BUTTON_MAX,0);
   scan_pad();assert(pad);key=SDL_CONTROLLER_BUTTON_DPAD_DOWN;
  }else if(step==5)key=SDL_CONTROLLER_BUTTON_DPAD_DOWN;
  else if(step==6&&selected==graphics_count+2)key=SDL_CONTROLLER_BUTTON_A;
 }else if(scenario==1){
  if(step==0&&page==DELETE_PROMPT){assert(selected==0);key=SDL_CONTROLLER_BUTTON_DPAD_DOWN;}
  else if(step==1&&selected==1)key=SDL_CONTROLLER_BUTTON_A;
  else if(step==2&&page==OPTIONS)key=SDL_CONTROLLER_BUTTON_B;
 }else if(scenario==2){
  if(!step&&page==BUSY&&access("started",F_OK)==0)key=SDL_CONTROLLER_BUTTON_B;
 }else if(scenario==3){
  if(!step&&page==ERROR_PAGE)key=SDL_CONTROLLER_BUTTON_A;
  else if(step==1&&page==OPTIONS)key=SDL_CONTROLLER_BUTTON_B;
 }else if(scenario==4||scenario==5){
  if(page==OPTIONS){
   if(step<graphics_count+2)key=SDL_CONTROLLER_BUTTON_DPAD_UP;
   else if(step==graphics_count+2){assert(selected==0);key=SDL_CONTROLLER_BUTTON_A;}
   else if(scenario==4)key=SDL_CONTROLLER_BUTTON_B;
   else if(step<2*graphics_count+5)key=SDL_CONTROLLER_BUTTON_DPAD_DOWN;
   else {assert(selected==graphics_count+2);key=SDL_CONTROLLER_BUTTON_A;}
  }
 } else if(scenario==6&&page==OPTIONS){
  if(step==0)key=SDL_CONTROLLER_BUTTON_DPAD_UP;
  else if(step==1){assert(frame_cap==60);key=SDL_CONTROLLER_BUTTON_A;}
  else if(step==2){assert(frame_cap==0);key=SDL_CONTROLLER_BUTTON_DPAD_RIGHT;}
  else if(step==3){assert(frame_cap==30);key=SDL_CONTROLLER_BUTTON_DPAD_RIGHT;}
  else if(step==4){assert(frame_cap==60);key=SDL_CONTROLLER_BUTTON_DPAD_LEFT;}
  else if(step==5){assert(frame_cap==30);key=SDL_CONTROLLER_BUTTON_DPAD_DOWN;}
  else if(step==6)key=SDL_CONTROLLER_BUTTON_A;
 }

 if(key<0)return 0;
 memset(e,0,sizeof *e);e->type=SDL_CONTROLLERBUTTONDOWN;e->cbutton.which=pad_id;e->cbutton.button=key;
 step++;next_action=now+80;return 1;
}
int main(int argc,char**argv){
 assert(argc==2);scenario=atoi(argv[1]);
 char root[]="/tmp/reconflict-launcher-test-XXXXXX";assert(mkdtemp(root));assert(!chdir(root));
 if(scenario>=4){assert(!mkdir("config",0755));FILE*c=fopen("config/graphics.cfg","w");assert(c);fputs("Shadows=0\n",c);fclose(c);}
 FILE*f=fopen("prepare_game.py","w");assert(f);
 fprintf(f,"from pathlib import Path\nimport sys,time\np=Path('calls')\nn=p.read_text() if p.exists() else ''\np.write_text(n+('delete' if '--delete-installer' in sys.argv else 'prepare')+'\\n')\nPath('started').touch()\n");
 if(scenario==2)fputs("time.sleep(60)\n",f);
 if(scenario>=4)fputs("sys.exit(0)\n",f);
 else if(scenario==3)fputs("sys.exit(0 if n else 1)\n",f);
 else fputs("sys.exit(0 if '--delete-installer' in sys.argv else 10)\n",f);
 fclose(f);
 char*args[]={"launcher","--root",root,"--handoff",NULL};
 setenv("SDL_VIDEODRIVER","offscreen",1);started=SDL_GetTicks();
 int code=launcher_main(4,args);assert(code==(scenario==0?20:(scenario==5||scenario==6)?21:0));
 f=fopen("calls","r");assert(f);char calls[80]={0};assert(fread(calls,1,sizeof calls-1,f)>0);fclose(f);
 assert(!strcmp(calls,scenario==1?"prepare\ndelete\n":scenario==3?"prepare\nprepare\n":"prepare\n"));
 if(scenario==0){assert(!load_fps()&&fps==0);unlink("config/launcher.cfg");unlink("config/graphics.cfg");unlink("config/launcher.cfg");rmdir("config");}
 if(scenario==6){assert(!load_fps()&&fps==1&&frame_cap==30);}
 if(scenario>=4){DSGraphicsConfig checked;ds_configuration_defaults(&checked);assert(ds_configuration_load(&checked)==1);
  assert(checked.value[2]==(unsigned)(scenario==5));
  if(scenario==4){char original[32]={0};f=fopen("config/graphics.cfg","r");assert(fread(original,1,31,f)==10);fclose(f);assert(!strcmp(original,"Shadows=0\n"));}
  unlink("config/graphics.cfg");unlink("config/launcher.cfg");rmdir("config");}
 unlink("prepare_game.py");unlink("calls");unlink("started");assert(!chdir("/tmp"));assert(!rmdir(root));
 printf("PASS launcher flow %d: controller navigation, resize, child handoff\n",scenario);
 return 0;
}
