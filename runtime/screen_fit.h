/* Screen fit: the game draws its backbuffer into an offscreen target of the
 * game's own size, which is scaled to the screen keeping the aspect (black
 * bars) on present. Only active when the screen is not the game's size. */
#ifndef DS_SCREEN_FIT_H
#define DS_SCREEN_FIT_H
#include <SDL.h>
/* After the GL context exists (on its thread). 0 on success or when not needed. */
int ds_screen_fit_init(SDL_Window *window,int game_width,int game_height);
/* The framebuffer the game's backbuffer lives in: the offscreen target, or 0. */
unsigned ds_screen_fit_target(void);
/* Before the swap, on the GL thread: scale the frame onto the screen. */
void ds_screen_fit_present(SDL_Window *window);
void ds_screen_fit_close(void);
#endif
