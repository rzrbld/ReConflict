/* Actual RT_BITMAP decoder differential fixture. Other API sections are removed
 * by --gc-sections, so this can run on the build host without SDL_ttf. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include "win32_extra.c"
uint32_t g_eax,g_esp;
ptrdiff_t g_mem_base;
static uint32_t resource_size,error_code;
static uint32_t *decoded;
static int decoded_width,decoded_height;
void hle_set_last_error(uint32_t value){error_code=value;}
uint32_t hle_find_resource(uint32_t module,uint32_t name,uint32_t type){assert(!module);assert(name==107 || name==109);return type==2?1:0;}
uint32_t hle_resource_data(uint32_t module,uint32_t resource,uint32_t *size){assert(!module);assert(resource==1);*size=resource_size;return 0x10000;}
uint32_t hle_gdi_make_bitmap(int w,int h,int bpp,uint8_t *pixels,int owns){assert(bpp==32 && owns);decoded=(uint32_t *)pixels;decoded_width=w;decoded_height=h;return 0x123456;}
int main(int argc,char **argv){
    if(argc!=4)return 2;
    FILE *f=fopen(argv[1],"rb");assert(f);assert(!fseek(f,0,SEEK_END));long bytes=ftell(f);assert(bytes>0 && bytes<0xe0000);rewind(f);
    g_mem_base=(ptrdiff_t)calloc(1,0x100000);assert(g_mem_base);resource_size=(uint32_t)bytes;
    assert(fread(hle_ptr(0x10000),1,resource_size,f)==resource_size);fclose(f);
    g_esp=0x1000;MEM32(g_esp)=0xdeadbeef;MEM32(g_esp+4)=0;MEM32(g_esp+8)=(uint32_t)atoi(argv[3]);
    x_LoadBitmapA();assert(g_eax==0x123456 && g_esp==0x100c && !error_code);
    f=fopen(argv[2],"wb");assert(f);assert(fwrite(decoded,4,(size_t)decoded_width*decoded_height,f)==(size_t)decoded_width*decoded_height);fclose(f);
    printf("Decoded RT_BITMAP %dx%d; stdcall purge PASS\n",decoded_width,decoded_height);free(decoded);free((void *)g_mem_base);return 0;
}
