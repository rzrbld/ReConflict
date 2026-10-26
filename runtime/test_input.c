/* Opt-in controller automation. Commands alter sampled controller input only;
 * game state still changes exclusively through the game's own input code. */
#include "test_input.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int initialized,enabled;
static FILE *commands;
static long offset;
static char pending[160];
static size_t used;
static int overflow;
static uint32_t button_mask,button_values,axis_mask;
static Sint16 axes[SDL_CONTROLLER_AXIS_MAX];
static SDL_GameControllerButton taps[32];
static unsigned tap_head,tap_count,tap_release;

static void command(const char *line){
    char kind[24],name[48],extra;int value;
    if(!strcmp(line,"reset")){button_mask=button_values=axis_mask=0;tap_head=tap_count=tap_release=0;fprintf(stderr,"[test-input] reset\n");return;}
    if(sscanf(line,"%23s %47s %c",kind,name,&extra)==2&&!strcmp(kind,"tap")){
        SDL_GameControllerButton b=SDL_GameControllerGetButtonFromString(name);
        if(b!=SDL_CONTROLLER_BUTTON_INVALID&&(unsigned)b<32&&tap_count<32){taps[(tap_head+tap_count)%32]=b;++tap_count;fprintf(stderr,"[test-input] queued tap %s\n",name);return;}
        fprintf(stderr,"[test-input] rejected tap: %s\n",line);return;
    }
    if(sscanf(line,"%23s %47s %d %c",kind,name,&value,&extra)!=3){fprintf(stderr,"[test-input] rejected command: %s\n",line);return;}
    if(!strcmp(kind,"button")&&(value==0||value==1)){
        SDL_GameControllerButton b=SDL_GameControllerGetButtonFromString(name);
        if(b!=SDL_CONTROLLER_BUTTON_INVALID&&(unsigned)b<32){uint32_t bit=1u<<b;button_mask|=bit;if(value)button_values|=bit;else button_values&=~bit;goto accepted;}
    }else if(!strcmp(kind,"axis")&&value>=-32768&&value<=32767){
        SDL_GameControllerAxis a=SDL_GameControllerGetAxisFromString(name);
        if(a!=SDL_CONTROLLER_AXIS_INVALID){axes[a]=(Sint16)value;axis_mask|=1u<<a;goto accepted;}
    }
    fprintf(stderr,"[test-input] rejected command: %s\n",line);return;
accepted:fprintf(stderr,"[test-input] %s\n",line);
}
void ds_test_input_pump(void){
    if(!initialized){
        initialized=1;const char *opt=getenv("DS_TEST_INPUT"),*root=getenv("DS_ROOT");
        if(!opt||strcmp(opt,"1"))return;
        if(!root){fprintf(stderr,"[test-input] DS_ROOT is required\n");return;}
        char path[PATH_MAX];if(snprintf(path,sizeof path,"%s/input.commands",root)>=(int)sizeof path)return;
        commands=fopen(path,"a+b");if(!commands){perror("[test-input] input.commands");return;}
        struct stat st;if(fstat(fileno(commands),&st)||!S_ISREG(st.st_mode)){fclose(commands);commands=NULL;return;}
        /* Existing commands belong to earlier runs; start from current EOF. */
        if(fseek(commands,0,SEEK_END)){fclose(commands);commands=NULL;return;}
        offset=ftell(commands);enabled=1;fprintf(stderr,"[test-input] enabled: %s (offset %ld)\n",path,offset);
    }
    if(!enabled)return;
    struct stat st;if(fstat(fileno(commands),&st))return;
    if(st.st_size<offset){offset=0;used=0;overflow=0;button_mask=button_values=axis_mask=0;tap_head=tap_count=tap_release=0;}
    clearerr(commands);if(fseek(commands,offset,SEEK_SET))return;
    /* Bound work per pump even if a producer appends a large command file. */
    for(unsigned count=0;count<4096;++count){
        int ch=fgetc(commands);if(ch==EOF)break;++offset;
        if(ch=='\n'){if(!overflow){pending[used]=0;if(used)command(pending);}else fprintf(stderr,"[test-input] rejected oversized command\n");used=0;overflow=0;}
        else if(ch!='\r'){if(used+1<sizeof pending)pending[used++]=(char)ch;else overflow=1;}
    }
}
void ds_test_input_sample(SDL_GameController *pad,const DS_PadConfig *config,DS_PadState *state){
    if(!enabled){ds_pad_sample(pad,config,state);return;}
    DS_PadRaw raw={{0},0};
    if(pad&&SDL_GameControllerGetAttached(pad)){
        for(int a=0;a<SDL_CONTROLLER_AXIS_MAX;++a)raw.axes[a]=SDL_GameControllerGetAxis(pad,(SDL_GameControllerAxis)a);
        for(int b=0;b<SDL_CONTROLLER_BUTTON_MAX;++b)if(SDL_GameControllerGetButton(pad,(SDL_GameControllerButton)b))raw.buttons|=1u<<b;
    }
    raw.buttons=(raw.buttons&~button_mask)|(button_values&button_mask);
    for(int a=0;a<SDL_CONTROLLER_AXIS_MAX;++a)if(axis_mask&(1u<<a))raw.axes[a]=axes[a];
    if(tap_count){
        SDL_GameControllerButton b=taps[tap_head];uint32_t bit=1u<<b;
        if(!tap_release){raw.buttons|=bit;tap_release=1;fprintf(stderr,"[test-input] tap %s consumed press\n",SDL_GameControllerGetStringForButton(b));}
        else{raw.buttons&=~bit;tap_release=0;tap_head=(tap_head+1)%32;--tap_count;fprintf(stderr,"[test-input] tap %s consumed release\n",SDL_GameControllerGetStringForButton(b));}
    }
    ds_pad_encode(&raw,config,state);
}
