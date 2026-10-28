/* Asset-free SDL bootstrap. Vurus first-run palette/modal style adapted with
 * permission under MIT; see licenses/Vurus.txt. Font from our configuration UI.
 * The extractor runs in a child process; no installer is executed as a program. */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <time.h>
#include "configuration.h"
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
    static const unsigned char slash[7] = {1,2,2,4,8,8,16};
    static const unsigned char question[7] = {14,17,1,2,4,0,4};
    SDL_Rect pixels[4096]; int count = 0;
    for (; *s; ++s, x += 6 * scale) {
        int index = *s >= 'A' && *s <= 'Z' ? *s - 'A' : *s >= '0' && *s <= '9' ? 26 + *s - '0' : -1;
        for (int row = 0; row < 7; ++row) {
            unsigned bits = index >= 0 ? glyph[index][row] :
                            *s == '-' && row == 3 ? 31 : *s == '.' && row == 6 ? 4 :
                            *s == ':' && (row == 2 || row == 5) ? 4 :
                            *s == '%' ? percent[row] : *s == '/' ? slash[row] :
                            *s == '?' ? question[row] : 0;
            for (int col = 0; col < 5 && count < 4096; ++col)
                if (bits & (1u << (4 - col))) pixels[count++] = (SDL_Rect){x + col * scale, y + row * scale, scale, scale};
        }
    }
    SDL_RenderFillRects(r, pixels, count);
}
static void color(SDL_Renderer *r, int red, int green, int blue) { SDL_SetRenderDrawColor(r, red, green, blue, 255); }



enum Page { BUSY, DELETE_PROMPT, ERROR_PAGE, OPTIONS, STARTING };
static SDL_GameController *pad;
static SDL_JoystickID pad_id=-1;
static int focused=1, axis_held, selected, fps=1, frame_cap=60, percent=-1, cancelling;
static enum Page page=BUSY;
static char message[192]="CHECKING GAME FILES";
static pid_t worker=-1;
static int deleting;
static Uint64 import_started;
static int graphics_count,graphics_indices[DS_GRAPHICS_COUNT];
static DSGraphicsConfig graphics;
static SDL_Thread *save_thread;
static SDL_atomic_t save_done;
static int handoff;
static volatile sig_atomic_t stopped;
static void stop_signal(int signo){(void)signo;stopped=1;}
static Uint64 monotonic_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (Uint64)t.tv_sec*1000+(Uint64)t.tv_nsec/1000000;}

static void scan_pad(void){
 if(pad && SDL_GameControllerGetAttached(pad))return;
 if(pad)SDL_GameControllerClose(pad);
 pad=NULL;pad_id=-1;axis_held=0;
 for(int i=0;i<SDL_NumJoysticks();i++)if(SDL_IsGameController(i)){
  pad=SDL_GameControllerOpen(i);if(pad){pad_id=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad));break;}
 }
}
static int save_fps(int value){
 if(mkdir("config",0755) && errno!=EEXIST)return -1;
 FILE *f=fopen("config/launcher.cfg.tmp","w");if(!f)return -1;
 int bad=fprintf(f,"fps_overlay=%d\nframe_cap=%d\n",value,frame_cap)<0;
 if(fflush(f)||fsync(fileno(f)))bad=1;
 if(fclose(f))bad=1;
 if(bad||rename("config/launcher.cfg.tmp","config/launcher.cfg")){unlink("config/launcher.cfg.tmp");return -1;}
 fps=value;return 0;
}
static int load_fps(void){
 FILE *f=fopen("config/launcher.cfg","r");if(!f)return errno==ENOENT?0:-1;
 char line[80],extra;int value,seen=0,ok=1,new_fps=fps,new_cap=60;
 while(fgets(line,sizeof line,f)){
  if(!strchr(line,'\n')&&!feof(f)){ok=0;break;}
  if(sscanf(line,"fps_overlay=%d %c",&value,&extra)==1&&(value==0||value==1)&&!(seen&1)){new_fps=value;seen|=1;}
  else if(sscanf(line,"frame_cap=%d %c",&value,&extra)==1&&(value==0||value==30||value==60)&&!(seen&2)){new_cap=value;seen|=2;}
  else {ok=0;break;}
 }
 if(ferror(f))ok=0;
 fclose(f);
 if(ok&&(seen&1)){fps=new_fps;frame_cap=new_cap;return 0;}return -1;
}
static int save_options(void *unused){
 (void)unused;Uint64 began=SDL_GetTicks64();
 fprintf(stderr,"[launcher] saving settings at %llu ms\n",(unsigned long long)began);
 int result=(mkdir("config",0755)&&errno!=EEXIST)||ds_configuration_save(&graphics)||save_fps(fps);
 fprintf(stderr,"[launcher] settings save took %llu ms; result=%d\n",(unsigned long long)(SDL_GetTicks64()-began),result);
 SDL_AtomicSet(&save_done,1);return result;
}
static void start_worker(int remove_installer){
 page=BUSY;percent=-1;selected=0;cancelling=0;deleting=remove_installer;
 import_started=SDL_GetTicks64();
 snprintf(message,sizeof message,"%s",remove_installer?"CHECKING INSTALLER BEFORE DELETION":"CHECKING GAME FILES");
 unlink("install-status.txt");
 worker=fork();
 if(worker==0){
  setpgid(0,0);
  if(remove_installer)execlp("python3","python3","./prepare_game.py","--root",".","--delete-installer",(char*)NULL);
  else execlp("python3","python3","./prepare_game.py","--root",".",(char*)NULL);
  _exit(127);
 }
 if(worker<0){page=ERROR_PAGE;snprintf(message,sizeof message,"COULD NOT START IMPORTER");}
 else setpgid(worker,worker);
}
static void graphics_open(void){
 ds_configuration_defaults(&graphics);
 int loaded=ds_configuration_load(&graphics);
 if(loaded<0){page=ERROR_PAGE;selected=0;snprintf(message,sizeof message,"INVALID GRAPHICS.CFG. EXISTING FILE KEPT.");return;}
 if(!loaded)graphics.present=(1u<<DS_GRAPHICS_COUNT)-1u;
 graphics_count=0;
 for(int i=0;i<DS_GRAPHICS_COUNT;i++)if(ds_graphics_options[i].editable){graphics_indices[graphics_count++]=i;graphics.present|=1u<<i;}
 selected=graphics_count+2;page=OPTIONS;
}
static void graphics_change(int direction){
 (void)direction;
 if(selected==graphics_count){fps=!fps;return;}
 if(selected==graphics_count+1){
  if(direction>0)frame_cap=frame_cap==30?60:frame_cap==60?0:30;
  else frame_cap=frame_cap==30?0:frame_cap==0?60:30;
  return;
 }
 if(selected>graphics_count)return;
 int i=graphics_indices[selected];graphics.present|=1u<<i;graphics.value[i]=!graphics.value[i];
}
static void read_progress(void){
 FILE *f=fopen("install-status.txt","r");if(!f)return;
 char line[32];if(fgets(line,sizeof line,f)&&fgets(message,sizeof message,f)){
  percent=atoi(line);message[strcspn(message,"\r\n")]=0;
  for(char *p=message;*p;p++)if(*p>='a'&&*p<='z')*p-=32;
 }fclose(f);
}
static void box(SDL_Renderer*r,int x,int y,int w,int h){SDL_Rect b={x,y,w,h};SDL_RenderFillRect(r,&b);}
static void gold(SDL_Renderer*r){color(r,235,173,45);}
static void label(SDL_Renderer*r,int x,int y,const char*s){gold(r);text(r,x,y,2,s);}
static void button(SDL_Renderer*r,int row,const char*s){
 int y=170+row*48;color(r,row==selected?61:31,row==selected?68:43,31);box(r,48,y,544,36);label(r,60,y+11,s);
}
static void wrapped(SDL_Renderer*r,const char*s,int y){
 size_t len=strlen(s),at=0;
 for(int row=0;row<4&&at<len;row++){
  size_t n=len-at;if(n>44){n=44;while(n>0&&s[at+n]!=' ')n--;if(!n)n=44;}
  char line[45];memcpy(line,s+at,n);line[n]=0;label(r,48,y+row*24,line);at+=n;while(s[at]==' ')at++;
 }
}
static void draw(SDL_Renderer*r){
 color(r,9,15,14);SDL_RenderClear(r);label(r,24,32,"RECONFLICT");
 if(page==STARTING){
  color(r,5,9,8);box(r,68,182,520,136);
  color(r,11,20,17);box(r,60,174,520,136);gold(r);
  SDL_Rect modal={60,174,520,136};SDL_RenderDrawRect(r,&modal);
  label(r,224,211,"GAME IS STARTING");
  label(r,248,257,"PLEASE WAIT");
  return;
 }
 color(r,5,9,8);box(r,36,106,584,296);color(r,11,20,17);box(r,28,98,584,296);
 color(r,122,107,51);
 for(int inset=0;inset<=3;inset+=3){SDL_Rect b={28+inset,98+inset,584-inset*2,296-inset*2};SDL_RenderDrawRect(r,&b);}
 const char *title=page==BUSY?"PREPARING GAME FILES":page==STARTING?"STARTING GAME":page==DELETE_PROMPT?"IMPORT COMPLETE":page==OPTIONS?"RECONFLICT OPTIONS":"GAME FILES NOT READY";
 color(r,11,20,17);box(r,142,96,356,20);label(r,154,99,title);
 if(page==OPTIONS){
  label(r,48,130,"640 X 480 - CONFIGURE BEFORE PLAYING");
  for(int row=0;row<graphics_count+4;row++){
   int y=155+row*25;color(r,row==selected?61:31,row==selected?68:43,31);box(r,48,y,544,23);
   if(row<graphics_count){int i=graphics_indices[row];label(r,60,y+6,ds_graphics_options[i].label);label(r,510,y+6,graphics.value[i]?"ON":"OFF");}
   else if(row==graphics_count){label(r,60,y+6,"FPS COUNTER");label(r,510,y+6,fps?"ON":"OFF");}
   else if(row==graphics_count+1){label(r,60,y+6,"FRAME CAP");label(r,474,y+6,frame_cap==0?"UNCAPPED":frame_cap==30?"30 FPS":"60 FPS");}
   else label(r,60,y+6,row==graphics_count+2?"SAVE AND RUN":"CANCEL");
  }
 }else if(page==DELETE_PROMPT){
  label(r,48,133,"DELETE THE INSTALLER TO FREE 452 MIB?");
  button(r,0,"KEEP INSTALLER");button(r,1,"DELETE INSTALLER");
  label(r,48,289,"YOUR EXTRACTED GAME FILES WILL STAY.");
 }else if(page==ERROR_PAGE){
  wrapped(r,message,135);label(r,48,255,"DETAILS: INSTALL.LOG / README.MD");
  color(r,selected==0?61:31,43,31);box(r,48,300,260,38);label(r,60,312,"A RETRY");
  color(r,selected==1?61:31,43,31);box(r,332,300,260,38);label(r,344,312,"B EXIT");
 }else{
  wrapped(r,cancelling?"STOPPING SAFELY. PLEASE WAIT.":message,140);
  Uint64 elapsed=(SDL_GetTicks64()-import_started)/1000;
  char elapsed_text[64];
  snprintf(elapsed_text,sizeof elapsed_text,"TIME ELAPSED  %llu:%02llu",
           (unsigned long long)(elapsed/60),(unsigned long long)(elapsed%60));
  label(r,48,238,elapsed_text);
  label(r,48,262,percent<0?"WORKING...":"CURRENT TASK");
  color(r,5,9,8);box(r,48,280,544,26);
  for(int i=0;i<66;i++){
   int pulse=(int)(SDL_GetTicks()/70)%72;
   if(percent>=0?i<percent*66/100:(i>=pulse-6&&i<pulse))gold(r);else color(r,47,58,33);
   for(int row=0;row<3;row++)box(r,50+i*8,284+row*6,5,4);
  }
  label(r,48,345,page==STARTING?"LOADING YOUR GAME. PLEASE WAIT.":"ORIGINAL INSTALLER KEPT UNTIL YOU CHOOSE");
 }
 label(r,32,425,page==BUSY?"B / ESC CANCEL":page==STARTING?"SETTINGS AND SAVES STAY BESIDE THE GAME":page==OPTIONS?"UP / DOWN SELECT    LEFT / RIGHT CHANGE":"DPAD SELECT    A CONFIRM    B BACK");
 if(page==OPTIONS)label(r,32,452,"A SELECT    B CANCEL WITHOUT SAVING");
}
static void shutdown_ui(SDL_Renderer*r,SDL_Window*w){if(pad)SDL_GameControllerClose(pad);pad=NULL;SDL_DestroyRenderer(r);SDL_DestroyWindow(w);SDL_Quit();}
static int launch_game(void){
 if(handoff)return 20+fps;
 setenv("DS_FPS_OVERLAY",fps?"1":"0",1);
 setenv("DS_FRAME_CAP",frame_cap==30?"30":frame_cap==60?"60":"0",1);
 execl("./desert-storm.aarch64","desert-storm.aarch64",(char*)NULL);
 perror("Starting game");return 1;
}
int main(int argc,char**argv){
 const char *root_override=NULL,*preview=NULL,*shot=NULL;int width=800,height=600;
 for(int i=1;i<argc;i++){
  if(!strcmp(argv[i],"--root")&&i+1<argc)root_override=argv[++i];
  else if(!strcmp(argv[i],"--preview")&&i+1<argc)preview=argv[++i];
  else if(!strcmp(argv[i],"--screenshot")&&i+1<argc)shot=argv[++i];
  else if(!strcmp(argv[i],"--size")&&i+1<argc){if(sscanf(argv[++i],"%dx%d",&width,&height)!=2||width<320||height<240)return 2;}
  else if(!strcmp(argv[i],"--configure")){ /* Legacy settings entry uses the same options window. */ }
  else if(!strcmp(argv[i],"--handoff"))handoff=1;
  else {fprintf(stderr,"Usage: %s [--configure] [--root DIR] [--preview options|import|delete|error|starting --screenshot FILE --size WxH]\n",argv[0]);return 2;}
 }
 char root[PATH_MAX];
 if(root_override){if(!realpath(root_override,root)){perror("root");return 1;}}
 else{ssize_t n=readlink("/proc/self/exe",root,sizeof root-1);if(n<0||n>=(ssize_t)sizeof root-1)return 1;root[n]=0;char*p=strrchr(root,'/');if(!p)return 1;*p=0;}
 if(chdir(root)){perror("chdir");return 1;}
 setenv("DS_ROOT",root,1);
 if(!preview&&load_fps()){fprintf(stderr,"Invalid config/launcher.cfg; preserved. Fix or move it aside.\n");return 1;}
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)){fprintf(stderr,"SDL: %s\n",SDL_GetError());return 1;}
 SDL_Window*w=SDL_CreateWindow("ReConflict Options",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,width,height,SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
 SDL_Renderer*r=w?SDL_CreateRenderer(w,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC):NULL;
 if(!r&&w)r=SDL_CreateRenderer(w,-1,SDL_RENDERER_SOFTWARE);
 if(!r){fprintf(stderr,"Window: %s\n",SDL_GetError());if(w)SDL_DestroyWindow(w);SDL_Quit();return 1;}
 SDL_SetWindowMinimumSize(w,320,240);SDL_RenderSetLogicalSize(r,640,480);scan_pad();
 if(preview){
  page=!strcmp(preview,"delete")?DELETE_PROMPT:!strcmp(preview,"error")?ERROR_PAGE:BUSY;
  if(!strcmp(preview,"graphics")||!strcmp(preview,"menu")||!strcmp(preview,"options")){ds_configuration_defaults(&graphics);graphics_count=0;
   for(int i=0;i<DS_GRAPHICS_COUNT;i++)if(ds_graphics_options[i].editable){graphics_indices[graphics_count++]=i;graphics.present|=1u<<i;}
   page=OPTIONS;selected=graphics_count+2;}
  percent=43;snprintf(message,sizeof message,"%s",page==ERROR_PAGE?"COPY YOUR GOG INSTALLER HERE. SEE README.":"EXTRACTING FILES 128 OF 297");
  if(!strcmp(preview,"starting")){page=STARTING;percent=-1;snprintf(message,sizeof message,"STARTING GAME. PLEASE WAIT...");}
  draw(r);int result=0;
  if(shot){int x,y;SDL_GetRendererOutputSize(r,&x,&y);SDL_RenderFlush(r);SDL_RenderSetLogicalSize(r,0,0);SDL_RenderSetViewport(r,NULL);SDL_Surface*s=SDL_CreateRGBSurfaceWithFormat(0,x,y,32,SDL_PIXELFORMAT_ARGB8888);
   if(!s||SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,s->pixels,s->pitch)||SDL_SaveBMP(s,shot)){fprintf(stderr,"Screenshot: %s\n",SDL_GetError());result=1;}if(s)SDL_FreeSurface(s);}
  SDL_RenderPresent(r);shutdown_ui(r,w);return result;
 }
 signal(SIGTERM,stop_signal);signal(SIGINT,stop_signal);
 start_worker(0);
 int exit_requested=0,run=0;
 while(!exit_requested||worker>0){
  int move=0,adjust=0,accept=0,back=stopped!=0;SDL_Event e;
  while(SDL_PollEvent(&e)){
   if(e.type==SDL_QUIT){back=1;stopped=1;}
   if(e.type==SDL_CONTROLLERDEVICEADDED||e.type==SDL_CONTROLLERDEVICEREMOVED)scan_pad();
   if(e.type==SDL_WINDOWEVENT){if(e.window.event==SDL_WINDOWEVENT_FOCUS_LOST)focused=0;if(e.window.event==SDL_WINDOWEVENT_FOCUS_GAINED)focused=1;}
   if(!focused)continue;
   if(e.type==SDL_KEYDOWN&&!e.key.repeat){switch(e.key.keysym.sym){case SDLK_UP:move=-1;break;case SDLK_DOWN:move=1;break;case SDLK_LEFT:if(page==OPTIONS)adjust=-1;else move=-1;break;case SDLK_RIGHT:if(page==OPTIONS)adjust=1;else move=1;break;case SDLK_RETURN:accept=1;break;case SDLK_ESCAPE:back=1;break;}}
   if(e.type==SDL_CONTROLLERBUTTONDOWN&&e.cbutton.which==pad_id){switch(e.cbutton.button){case SDL_CONTROLLER_BUTTON_DPAD_UP:move=-1;break;case SDL_CONTROLLER_BUTTON_DPAD_DOWN:move=1;break;case SDL_CONTROLLER_BUTTON_DPAD_LEFT:if(page==OPTIONS)adjust=-1;else move=-1;break;case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:if(page==OPTIONS)adjust=1;else move=1;break;case SDL_CONTROLLER_BUTTON_A:accept=1;break;case SDL_CONTROLLER_BUTTON_B:back=1;break;}}
   if(e.type==SDL_MOUSEBUTTONDOWN&&e.button.button==SDL_BUTTON_LEFT){
    /* SDL_RenderSetLogicalSize transforms mouse events into logical coordinates. */
    int x=e.button.x,y=e.button.y;
    if(page==DELETE_PROMPT&&x>=48&&x<592&&y>=170){int row=(y-170)/48;
     if(row<2&&(y-170)%48<36){selected=row;accept=1;}}
    if(page==ERROR_PAGE&&y>=300&&y<338){if(x>=48&&x<308){selected=0;accept=1;}if(x>=332&&x<592)back=1;}
    if(page==OPTIONS&&x>=48&&x<592&&y>=155){int row=(y-155)/25;
     if(row<graphics_count+4&&(y-155)%25<23){selected=row;accept=1;}}
   }
  }
  if(pad&&focused){int y=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTY);
   if(abs(y)<8000)axis_held=0;else if(!axis_held&&abs(y)>16000){move=y<0?-1:1;axis_held=1;}}
  if(page==STARTING){
   if(SDL_AtomicGet(&save_done)){
    int result;SDL_WaitThread(save_thread,&result);save_thread=NULL;
    if(result){snprintf(message,sizeof message,"COULD NOT SAVE SETTINGS. CHECK CONFIG FOLDER.");page=ERROR_PAGE;selected=0;}
    else{snprintf(message,sizeof message,"STARTING GAME. PLEASE WAIT...");run=!stopped;exit_requested=1;}
   }
  }else if(page==BUSY){
   if(back&&!cancelling){cancelling=1;exit_requested=1;if(worker>0)kill(-worker,SIGTERM);}
   read_progress();int status;pid_t result=worker>0?waitpid(worker,&status,WNOHANG):0;
   if(result==worker&&worker>0){worker=-1;read_progress();int code=WIFEXITED(status)?WEXITSTATUS(status):1;
    if(!exit_requested){selected=0;page=code==10?DELETE_PROMPT:ERROR_PAGE;if(code==0)graphics_open();
     if(code==127)snprintf(message,sizeof message,"PYTHON3 MISSING. CHECK PORT REQUIREMENTS.");
     if(deleting&&code==0)snprintf(message,sizeof message,"INSTALLER DELETED");}}
  }else{
   int count=page==OPTIONS?graphics_count+4:2;selected=(selected+move+count)%count;
   if(adjust&&page==OPTIONS)graphics_change(adjust);
   if(back){if(!stopped&&page==DELETE_PROMPT)graphics_open();else exit_requested=1;}
   else if(accept){
    if(page==DELETE_PROMPT){if(selected==1)start_worker(1);else graphics_open();}
    else if(page==ERROR_PAGE){if(!selected)start_worker(0);else exit_requested=1;}
    else if(selected<=graphics_count+1)graphics_change(1);
    else if(selected==graphics_count+2){
     page=STARTING;percent=-1;snprintf(message,sizeof message,"SAVING SETTINGS...");SDL_AtomicSet(&save_done,0);
     save_thread=SDL_CreateThread(save_options,"save-options",NULL);
     if(!save_thread){page=ERROR_PAGE;selected=0;snprintf(message,sizeof message,"COULD NOT START SETTINGS SAVE.");}
    }else exit_requested=1;
   }
  }
  draw(r);SDL_RenderPresent(r);SDL_Delay(16);
 }
 Uint64 closing=monotonic_ms();fprintf(stderr,"[launcher] closing SDL; run=%d\n",run);
 shutdown_ui(r,w);fprintf(stderr,"[launcher] SDL close took %llu ms; handing off\n",(unsigned long long)(monotonic_ms()-closing));
 return run?launch_game():0;
}
