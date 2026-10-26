/* Win32 bridge extensions needed by Conflict: Desert Storm.
 * All guest pointers/handles retain their 32-bit ABI on AArch64.
 * Optional shell/file selection uses explicit host selections, otherwise
 * returns the documented cancellation result; it does not fabricate a file.
 */
#define _POSIX_C_SOURCE 200809L
#include "win32_extra.h"
#include "configuration.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define ERROR_INVALID_HANDLE 6u
#define ERROR_NOT_ENOUGH_MEMORY 8u
#define ERROR_INVALID_PARAMETER 87u
#define ERROR_CALL_NOT_IMPLEMENTED 120u
#define ERROR_FILENAME_EXCED_RANGE 206u

extern int ds_hle_gdi_get_object(uint32_t object, int count, void *out);
extern int ds_hle_gdi_draw_text_utf8(uint32_t dc, const char *text, int32_t *rect, uint32_t flags);
extern uint32_t hle_gdi_make_bitmap(int w,int h,int bpp,uint8_t *pixels,int owns);

static void delegate(const char *name) {
    uint32_t va = hle_resolve(name);
    recomp_func_t fn = va ? recomp_lookup_import(va) : NULL;
    if (!fn) hle_fatal("Win32 extension needs registered %s", name);
    fn();
}

/* UTF-16 -> UTF-8 in guest scratch. Atoms in the resource API are preserved. */
static uint32_t utf8(uint32_t wide, int count, int atoms) {
    if (!wide || (atoms && wide < 0x10000u)) return wide;
    size_t n = count < 0 ? 32768u : (size_t)count;
    if (n > 32768u) { hle_set_last_error(ERROR_FILENAME_EXCED_RANGE); return 0; }
    char *out = hle_ptr(hle_scratch((uint32_t)(n * 4 + 1)));
    if (!out) { hle_set_last_error(ERROR_NOT_ENOUGH_MEMORY); return 0; }
    size_t used = 0;
    for (size_t i = 0; i < n; ++i) {
        uint32_t c = MEM16(wide + (uint32_t)i * 2);
        if (!c && count < 0) { out[used] = 0; return hle_va(out); }
        if (c >= 0xd800u && c <= 0xdbffu && i + 1 < n) {
            uint32_t lo = MEM16(wide + (uint32_t)(i + 1) * 2);
            if (lo >= 0xdc00u && lo <= 0xdfffu) { c = 0x10000u + ((c - 0xd800u) << 10) + lo - 0xdc00u; ++i; }
            else c = 0xfffdu;
        } else if (c >= 0xd800u && c <= 0xdfffu) c = 0xfffdu;
        if (c < 0x80) out[used++] = (char)c;
        else if (c < 0x800) { out[used++] = (char)(0xc0u | c >> 6); out[used++] = (char)(0x80u | (c & 63)); }
        else if (c < 0x10000) { out[used++] = (char)(0xe0u | c >> 12); out[used++] = (char)(0x80u | ((c >> 6) & 63)); out[used++] = (char)(0x80u | (c & 63)); }
        else { out[used++] = (char)(0xf0u | c >> 18); out[used++] = (char)(0x80u | ((c >> 12) & 63)); out[used++] = (char)(0x80u | ((c >> 6) & 63)); out[used++] = (char)(0x80u | (c & 63)); }
    }
    out[used] = 0;
    if (count < 0) { hle_set_last_error(ERROR_FILENAME_EXCED_RANGE); return 0; }
    return hle_va(out);
}

static void x_CreateFileW(void) {
    uint32_t mark = hle_scratch_mark(), esp = g_esp, original = A32(0);
    uint32_t converted = utf8(original, -1, 0);
    if (!converted) { hle_scratch_release(mark); RET(UINT32_MAX, 7); }
    MEM32(esp + 4) = converted;
    delegate("CreateFileA");
    MEM32(esp + 4) = original;
    hle_scratch_release(mark);
}
static void x_FindResourceW(void) {
    uint32_t mark = hle_scratch_mark(), esp = g_esp, name = A32(1), type = A32(2);
    uint32_t n = utf8(name, -1, 1), t = utf8(type, -1, 1);
    if (!n || !t) { hle_scratch_release(mark); RET(0, 3); }
    MEM32(esp + 8) = n; MEM32(esp + 12) = t;
    delegate("FindResourceA");
    MEM32(esp + 8) = name; MEM32(esp + 12) = type;
    hle_scratch_release(mark);
}
static void x_GetObjectA(void) {
    int result = ds_hle_gdi_get_object(A32(0), (int)A32(1), APTR(2));
    if (!result) hle_set_last_error(ERROR_INVALID_HANDLE);
    RET(result, 3);
}
static uint32_t bitmap_component(uint32_t pixel,uint32_t mask) {
    if(!mask)return 0;
    unsigned shift=0;while(!(mask&1u)){mask>>=1;++shift;}
    return (uint32_t)((uint64_t)(pixel>>shift & mask)*255u/mask);
}
static void x_LoadBitmapA(void) {
    uint32_t resource=hle_find_resource(A32(0),A32(1),2),size=0;
    uint32_t va=resource?hle_resource_data(A32(0),resource,&size):0;
    if(!va || size<40) {hle_set_last_error(1814);RET(0,2);}
    uint32_t header=MEM32(va),compression=MEM32(va+16),colors=MEM32(va+32);
    int32_t width=(int32_t)MEM32(va+4),signed_height=(int32_t)MEM32(va+8);
    unsigned depth=MEM16(va+14);
    if(header<40 || header>size || width<1 || width>8192 || !signed_height || signed_height==INT32_MIN ||
       (compression!=0 && compression!=3) || (depth!=1 && depth!=4 && depth!=8 && depth!=16 && depth!=24 && depth!=32)) {
        hle_set_last_error(50);RET(0,2);
    }
    unsigned height=(unsigned)(signed_height<0?-signed_height:signed_height);
    if(height>8192) {hle_set_last_error(8);RET(0,2);}
    uint32_t masks[3]={depth==16?0x7c00u:0xff0000u,depth==16?0x03e0u:0x00ff00u,depth==16?0x001fu:0x0000ffu};
    uint32_t offset=header;
    if(compression==3) {
        if(depth!=16 && depth!=32) {hle_set_last_error(50);RET(0,2);}
        uint32_t at=header>=52?40:header;
        if((uint64_t)at+12>size) {hle_set_last_error(13);RET(0,2);}
        for(unsigned i=0;i<3;++i)masks[i]=MEM32(va+at+i*4);
        if(header==40)offset+=12;
    }
    if(depth<=8) {if(!colors)colors=1u<<depth;if(colors>(1u<<depth)){hle_set_last_error(13);RET(0,2);}}
    else colors=0;
    uint32_t palette=offset;offset+=colors*4;
    size_t pitch=(((size_t)width*depth+31)/32)*4;
    if((uint64_t)offset+pitch*height>size) {hle_set_last_error(13);RET(0,2);}
    uint32_t *pixels=malloc((size_t)width*height*4);
    if(!pixels) {hle_set_last_error(8);RET(0,2);}
    const uint8_t *src=hle_ptr(va);
    for(unsigned y=0;y<height;++y) {
        const uint8_t *row=src+offset+(signed_height>0?height-1-y:y)*pitch;
        for(int x=0;x<width;++x) {
            uint32_t rgb=0;
            if(depth<=8) {
                unsigned index=depth==8?row[x]:depth==4?(row[x/2]>>(x%2?0:4))&15:(row[x/8]>>(7-x%8))&1;
                if(index<colors)memcpy(&rgb,src+palette+index*4,4);
            } else if(depth==24)rgb=(uint32_t)row[x*3+2]<<16|(uint32_t)row[x*3+1]<<8|row[x*3];
            else {
                uint32_t pixel=0;memcpy(&pixel,row+(size_t)x*(depth/8),depth/8);
                rgb=bitmap_component(pixel,masks[0])<<16|bitmap_component(pixel,masks[1])<<8|bitmap_component(pixel,masks[2]);
            }
            pixels[(size_t)y*width+x]=0xff000000u|(rgb&0xffffffu);
        }
    }
    uint32_t handle=hle_gdi_make_bitmap(width,(int)height,32,(uint8_t *)pixels,1);
    if(!handle)free(pixels);
    RET(handle,2);
}
static void x_DrawTextW(void) {
    uint32_t mark = hle_scratch_mark(), va = utf8(A32(1), (int)A32(2), 0);
    if (!va || !A32(3)) { hle_scratch_release(mark); RET(0, 5); }
    int result = ds_hle_gdi_draw_text_utf8(A32(0), hle_ptr(va), APTR(3), A32(4));
    hle_scratch_release(mark);
    RET(result, 5);
}
static void x_DestroyCursor(void) {
    uint32_t h = A32(0);
    /* Resource cursor images are owned by the mapped image, not an OS allocation.
     * There is no native resource to release; unknown handles still fail. */
    int known = h && (hle_cursor_image(h) || h == hle_current_cursor());
    if (!known) hle_set_last_error(ERROR_INVALID_HANDLE);
    RET(known, 1);
}

static uint32_t screensaver_active, lowpower_active, poweroff_active;
static int32_t mouse_parameters[3] = {6,10,0};
static void x_SystemParametersInfoA(void) {
    uint32_t action = A32(0), param = A32(1), pv = A32(2);
    uint32_t *value = NULL;
    switch (action) {
        case 0x10: value = &screensaver_active; break;  /* SPI_GETSCREENSAVEACTIVE */
        case 0x53: value = &lowpower_active; break;
        case 0x54: value = &poweroff_active; break;
        case 0x11: screensaver_active = param != 0; RET(1,4);
        case 0x55: lowpower_active = param != 0; RET(1,4);
        case 0x56: poweroff_active = param != 0; RET(1,4);
        case 0x03:
            if (!pv) break;
            memcpy(hle_ptr(pv), mouse_parameters, sizeof mouse_parameters); RET(1,4);
        case 0x04:
            if (!pv) break;
            memcpy(mouse_parameters, hle_ptr(pv), sizeof mouse_parameters); RET(1,4);
        case 0x30: /* SPI_GETWORKAREA: handheld is the whole 640x480 screen. */
            if (!pv) break;
            MEM32(pv)=0; MEM32(pv+4)=0; MEM32(pv+8)=640; MEM32(pv+12)=480; RET(1,4);
        default:
            fprintf(stderr,"[win32] unsupported SystemParametersInfoA action %#x\n",action);
            hle_set_last_error(ERROR_CALL_NOT_IMPLEMENTED); RET(0,4);
    }
    if (value && pv) { MEM32(pv)=*value; RET(1,4); }
    hle_set_last_error(ERROR_INVALID_PARAMETER); RET(0,4);
}

static _Thread_local int com_initialized, com_mode;
static void x_CoInitializeEx(void) {
    uint32_t flags = A32(1);
    if (A32(0) || flags & ~0xeu) RET(0x80070057u,2); /* E_INVALIDARG */
    int mode = (flags & 2u) != 0;
    if (com_initialized && com_mode != mode) RET(0x80010106u,2); /* RPC_E_CHANGED_MODE */
    int prior = com_initialized++;
    com_mode = mode;
    RET(prior ? 1u : 0u,2); /* S_FALSE / S_OK */
}
static void x_CoUninitialize(void) { if (com_initialized) --com_initialized; RETV(0); }

static void shell_malloc_query(void) {
    static const uint8_t iid_unknown[16]={0,0,0,0,0,0,0,0,0xc0,0,0,0,0,0,0,0x46};
    static const uint8_t iid_malloc[16]={2,0,0,0,0,0,0,0,0xc0,0,0,0,0,0,0,0x46};
    if(!A32(2)) RET(0x80004003u,3);
    MEM32(A32(2))=0;
    if(!A32(1) || (memcmp(APTR(1),iid_unknown,16) && memcmp(APTR(1),iid_malloc,16))) RET(0x80004002u,3);
    hle_com_QueryInterface();
}
static void shell_malloc_alloc(void) { RETP(hle_alloc(A32(1)),2); }
static void shell_malloc_realloc(void) { RETP(hle_heap_realloc(APTR(1),A32(2),0),3); }
static void shell_malloc_free(void) { hle_free(APTR(1)); RETV(2); }
static void shell_malloc_size(void) { RET(hle_heap_ok(APTR(1)) ? hle_heap_size(APTR(1)) : UINT32_MAX,2); }
static void shell_malloc_didalloc(void) { RET(hle_heap_ok(APTR(1)),2); }
static void shell_malloc_minimize(void) { RETV(1); }
static const win32hle_shim shell_malloc_methods[] = {
    {"DS.IMalloc.QueryInterface",shell_malloc_query},{"DS.IMalloc.AddRef",hle_com_AddRef},
    {"DS.IMalloc.Release",hle_com_Release},{"DS.IMalloc.Alloc",shell_malloc_alloc},
    {"DS.IMalloc.Realloc",shell_malloc_realloc},{"DS.IMalloc.Free",shell_malloc_free},
    {"DS.IMalloc.GetSize",shell_malloc_size},{"DS.IMalloc.DidAlloc",shell_malloc_didalloc},
    {"DS.IMalloc.HeapMinimize",shell_malloc_minimize},{0,0}};
static void x_SHGetMalloc(void) {
    if (!A32(0)) RET(0x80070057u,1);
    static uint32_t vt;
    if (!vt) vt=hle_com_vtable(shell_malloc_methods);
    MEM32(A32(0))=hle_com_new(vt,8);
    RET(MEM32(A32(0)) ? 0 : 0x8007000eu,1);
}
static void x_SHBrowseForFolder(void) {
    const char *path=getenv("DS_BROWSE_FOLDER");
    if (!path || !*path) RET(0,1); /* User selection was not supplied: cancelled. */
    char host[1024]; struct stat st;
    if (!hle_host_path(path,host,sizeof host) || stat(host,&st) || !S_ISDIR(st.st_mode)) {
        hle_set_last_error(ERROR_INVALID_PARAMETER); RET(0,1);
    }
    size_t n=strlen(path); if(n>259) {hle_set_last_error(ERROR_FILENAME_EXCED_RANGE);RET(0,1);}
    uint8_t *pidl=hle_alloc((uint32_t)n+9);
    if (!pidl) RET(0,1);
    uint32_t va=hle_va(pidl);
    MEM16(va)=(uint16_t)(n+7); MEM32(va+2)=0x53465344u; memcpy(pidl+6,path,n+1); MEM16(va+(uint32_t)n+7)=0;
    uint32_t bi=A32(0);
    if (bi && MEM32(bi+8)) {
        const char *base=strrchr(path,'/'); base=base ? base+1 : path;
        snprintf(hle_ptr(MEM32(bi+8)),260,"%s",base);
    }
    RET(va,1);
}
static void x_SHGetPathFromIDList(void) {
    uint32_t va=A32(0), out=A32(1);
    if (!va || !out || MEM16(va)<7 || MEM16(va)>267 || MEM32(va+2)!=0x53465344u) RET(0,2);
    unsigned n=MEM16(va)-7;
    memcpy(hle_ptr(out),hle_ptr(va+6),n); MEM8(out+n)=0; RET(1,2);
}

static _Thread_local uint32_t dialog_error;
static void file_selection(int save) {
    uint32_t ofn=A32(0); dialog_error=0;
    if (!ofn || MEM32(ofn)<76) {dialog_error=1;RET(0,1);}
    const char *selection=getenv(save ? "DS_SAVE_FILE" : "DS_OPEN_FILE");
    if (!selection || !*selection) RET(0,1); /* Documented user-cancel result. */
    uint32_t file=MEM32(ofn+28), cap=MEM32(ofn+32);
    size_t n=strlen(selection);
    if (!file || cap<n+1) {
        if(file && cap>=2) MEM16(file)=(uint16_t)(n+1);
        dialog_error=0x3003;RET(0,1); /* FNERR_BUFFERTOOSMALL */
    }
    if(!save) {
        char host[1024]; struct stat st;
        if(!hle_host_path(selection,host,sizeof host) || stat(host,&st) || !S_ISREG(st.st_mode)) {
            hle_set_last_error(2);RET(0,1);
        }
    }
    memcpy(hle_ptr(file),selection,n+1);
    const char *base=selection;
    for(const char *p=selection;*p;++p)if(*p=='/' || *p=='\\')base=p+1;
    MEM16(ofn+56)=(uint16_t)(base-selection);
    const char *ext=strrchr(base,'.'); MEM16(ofn+58)=ext ? (uint16_t)(ext-selection+1) : 0;
    uint32_t title=MEM32(ofn+36),titlecap=MEM32(ofn+40);
    if(title && titlecap)snprintf(hle_ptr(title),titlecap,"%s",base);
    RET(1,1);
}
static void x_GetOpenFileNameA(void){file_selection(0);}
static void x_GetSaveFileNameA(void){file_selection(1);}
static void x_CommDlgExtendedError(void){RET(dialog_error,0);}

const win32hle_shim ds_win32_extra_shims[] = {
    {"CreateFileW",x_CreateFileW},{"FindResourceW",x_FindResourceW},{"GetObjectA",x_GetObjectA},{"LoadBitmapA",x_LoadBitmapA},
    {"DrawTextW",x_DrawTextW},{"DestroyCursor",x_DestroyCursor},{"SystemParametersInfoA",x_SystemParametersInfoA},
    {"CoInitializeEx",x_CoInitializeEx},{"CoUninitialize",x_CoUninitialize},
    {"SHGetMalloc",x_SHGetMalloc},{"SHBrowseForFolder",x_SHBrowseForFolder},{"SHBrowseForFolderA",x_SHBrowseForFolder},
    {"SHGetPathFromIDList",x_SHGetPathFromIDList},{"SHGetPathFromIDListA",x_SHGetPathFromIDList},
    {"GetOpenFileNameA",x_GetOpenFileNameA},{"GetSaveFileNameA",x_GetSaveFileNameA},
    {"CommDlgExtendedError",x_CommDlgExtendedError},{0,0}};

void ds_seed_game_settings(void) {
    /* Registry names verified from original read 0x447cc0/write 0x448570.
     * Index 0 selects the first enumerated native adapter/display mode. The
     * D3D8 facade must enumerate 640x480 first; this is not a guessed pixel size.
     * Loaded user values win over these conservative first-launch defaults. */
    const char *key=DS_GRAPHICS_REGISTRY_KEY;
    DSGraphicsConfig config;
    ds_configuration_defaults(&config);
    ds_configuration_load(&config);
    uint32_t open=hle_resolve("RegOpenKeyExA"),query=hle_resolve("RegQueryValueExA"),close=hle_resolve("RegCloseKey");
    if(!open || !query || !close)hle_fatal("Register registry HLE before seeding Desert Storm settings");
    uint32_t mark=hle_scratch_mark();
    uint32_t path=hle_scratch_copy(key,(uint32_t)strlen(key)+1),out=hle_scratch(4);
    uint32_t args[6]={0x80000002u,path,0,0x20019u,out,0};
    uint32_t opened=hle_call_guest(open,5,args),handle=opened?0:MEM32(out);
    for(size_t i=0;i<DS_GRAPHICS_COUNT;++i) {
        int exists=0;
        if(handle) {
            uint32_t name=hle_scratch_copy(ds_graphics_options[i].key,(uint32_t)strlen(ds_graphics_options[i].key)+1);
            uint32_t q[6]={handle,name,0,0,0,0};
            exists=hle_call_guest(query,6,q)==0;
        }
        if(!exists || (config.present & (1u<<i)))hle_reg_set_dword(0x80000002u,key,ds_graphics_options[i].key,config.value[i]);
    }
    if(handle) {uint32_t a[1]={handle};hle_call_guest(close,1,a);}
    hle_scratch_release(mark);
}

void ds_startup_dialog_hook(uint32_t hwnd,uint32_t resource_id) {
    if(resource_id==106u && !getenv("DS_SHOW_WIN32_LAUNCHER")) {
        fprintf(stderr,"[startup] Applying selected SDL2/native mode through original launcher IDOK\n");
        hle_post_message(hwnd,0x0111u,1u,0); /* WM_COMMAND, IDOK */
    }
}
