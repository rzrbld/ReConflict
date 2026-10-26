#ifndef DS_FPS_OVERLAY_H
#define DS_FPS_OVERLAY_H
#include <SDL.h>
void ds_fps_overlay_draw(SDL_Window *window);
void ds_fps_overlay_presented(void);
void ds_fps_overlay_close(void);
#endif
