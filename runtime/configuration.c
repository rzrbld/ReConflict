#define _POSIX_C_SOURCE 200809L
#include "configuration.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>
/* Original dialog confirms texture0=High/1=Low; ShrubLevel is the
 * inverted-quality checkbox "Less shrubs", so1 is the minimum preset. */
const DSGraphicsOption ds_graphics_options[DS_GRAPHICS_COUNT]={
 {"AdapterIndex","ADAPTER",0,0,0}, {"ResolutionIndex","640 X 480",0,0,0},
 {"Shadows","SHADOWS",0,1,1}, {"PShaders","PIXEL SHADERS",0,0,0},
 {"VShaders","VERTEX SHADERS",0,0,0}, {"DrawDistance","DRAW DISTANCE",0,0,0},
 {"TextureResolution","LOW TEXTURES",1,1,1}, {"ShrubLevel","LESS SHRUBS",1,1,0},
 {"DetailTextures","DETAIL TEXTURES",0,1,1}, {"Decals","DECALS",0,1,1},
 {"EnvironmentMaps","ENVIRONMENT MAPS",0,1,1}
};
void ds_configuration_defaults(DSGraphicsConfig *s){
 memset(s,0,sizeof *s);
 for(int i=0;i<DS_GRAPHICS_COUNT;i++)s->value[i]=ds_graphics_options[i].initial;
}
static int config_path(char *out,size_t size,const char *suffix){
 const char *root=getenv("DS_ROOT");
 if(!root || !*root){errno=EINVAL;return -1;}
 int n=snprintf(out,size,"%s/config/graphics.cfg%s",root,suffix);
 if(n<0 || (size_t)n>=size){errno=ENAMETOOLONG;return -1;}return 0;
}
static char *trim(char *p){while(isspace((unsigned char)*p))++p;char *e=p+strlen(p);while(e>p&&isspace((unsigned char)e[-1]))--e;*e=0;return p;}
int ds_configuration_load(DSGraphicsConfig *s){
 char path[PATH_MAX];if(config_path(path,sizeof path,""))return -1;
 FILE *f=fopen(path,"r");if(!f)return errno==ENOENT?0:-1;
 DSGraphicsConfig candidate=*s;char line[256];unsigned number=0;int ok=1;
 while(fgets(line,sizeof line,f)){
  ++number;if(!strchr(line,'\n')&&!feof(f)){ok=0;break;}
  char *key=trim(line);if(!*key||*key=='#')continue;
  char *eq=strchr(key,'=');if(!eq){ok=0;break;}*eq=0;key=trim(key);char *val=trim(eq+1),*end;
  errno=0;unsigned long value=strtoul(val,&end,10);
  if(!*val||*val=='-'||errno||*trim(end)){ok=0;break;}
  int i;for(i=0;i<DS_GRAPHICS_COUNT;i++)if(!strcmp(key,ds_graphics_options[i].key))break;
  if(i==DS_GRAPHICS_COUNT||value>ds_graphics_options[i].maximum){ok=0;break;}
  candidate.value[i]=(uint32_t)value;candidate.present|=1u<<i;
 }
 if(ferror(f))ok=0;
 fclose(f);
 if(!ok){fprintf(stderr,"[configuration] invalid graphics.cfg line%u; ignoring entire file\n",number);errno=EINVAL;return -1;}
 *s=candidate;return 1;
}
int ds_configuration_save(const DSGraphicsConfig *s){
 char path[PATH_MAX],temp[PATH_MAX];if(config_path(path,sizeof path,"")||config_path(temp,sizeof temp,".tmp"))return -1;
 for(int i=0;i<DS_GRAPHICS_COUNT;i++)if((s->present&(1u<<i))&&s->value[i]>ds_graphics_options[i].maximum){errno=EINVAL;return -1;}
 FILE *f=fopen(temp,"w");if(!f)return -1;
 fputs("# Desert Storm native graphics setup\n# Omitted keys preserve existing registry values.\n# Current adapter mode: 640x480, GLES fixed-function translation.\n",f);
 for(int i=0;i<DS_GRAPHICS_COUNT;i++)if(s->present&(1u<<i))fprintf(f,"%s=%u\n",ds_graphics_options[i].key,s->value[i]);
 int failed=fflush(f)!=0;
 if(!failed&&fsync(fileno(f)))failed=1;
 if(fclose(f))failed=1;
 if(failed){unlink(temp);return -1;}
 if(rename(temp,path)){unlink(temp);return -1;}return 0;
}
