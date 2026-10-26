#ifndef DS_LOADING_BACKGROUND_H
#define DS_LOADING_BACKGROUND_H
#include <GLES2/gl2.h>
typedef struct {GLuint texture,program,buffer;int width,height,valid;} DsLoadingBackground;
/* Context-owner thread only. Save before Present, restore before progress/text.
 * No guest data, retained pointers, per-gameplay-frame hook or CPU readback. */
int ds_loading_background_capture(DsLoadingBackground *,int width,int height);
int ds_loading_background_restore(DsLoadingBackground *,int width,int height);
void ds_loading_background_close(DsLoadingBackground *);
#endif
