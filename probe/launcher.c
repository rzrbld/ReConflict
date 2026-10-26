/* Native SDL2 bring-up frontend; it does not start an unbuilt game runtime. */
#define _POSIX_C_SOURCE 200809L
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "desert_pad.h"
#include "../runtime/paths.h"

typedef struct { int es, vsync, deadzone, trigger, seconds, audio; } Settings;
static const char *keys[] = {"video.gles", "video.vsync", "controller.deadzone",
                            "controller.trigger_threshold", "probe.seconds", "probe.audio"};
static int *fields(Settings *s, int i) {
    switch (i) {
        case 0: return &s->es; case 1: return &s->vsync; case 2: return &s->deadzone;
        case 3: return &s->trigger; case 4: return &s->seconds; default: return &s->audio;
    }
}
static int valid(int i, int v) {
    return i == 0 ? v == 2 || v == 3 : i == 2 || i == 3 ? v >= 0 && v <= 90 :
           i == 4 ? v >= 2 && v <= 120 : v == 0 || v == 1;
}
static void load(Settings *s) {
    *s = (Settings){3, 1, 22, 15, 8, 1};
    FILE *f = fopen("options.cfg", "r"); if (!f) return;
    char line[160], key[80]; int value;
    while (fgets(line, sizeof(line), f))
        if (sscanf(line, "%79[^=]=%d", key, &value) == 2)
            for (int i = 0; i < 6; ++i) if (!strcmp(key, keys[i]) && valid(i, value)) *fields(s, i) = value;
    fclose(f);
}
static int save(Settings *s) {
    FILE *f = fopen("options.cfg.tmp", "w"); if (!f) return 0;
    fprintf(f, "# Native SDL2 setup. Game runtime is not built.\nvideo.width=640\nvideo.height=480\nvideo.fullscreen=1\n");
    for (int i = 0; i < 6; ++i) fprintf(f, "%s=%d\n", keys[i], *fields(s, i));
    int ok = !ferror(f);
    if (fclose(f)) ok = 0;
    return ok && !rename("options.cfg.tmp", "options.cfg");
}

/* Original 5x7 uppercase bitmap font; no font or SDL_ttf runtime dependency. */
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
    SDL_Rect pixels[4096]; int count = 0;
    for (; *s; ++s, x += 6 * scale) {
        int index = *s >= 'A' && *s <= 'Z' ? *s - 'A' : *s >= '0' && *s <= '9' ? 26 + *s - '0' : -1;
        for (int row = 0; row < 7; ++row) {
            unsigned bits = index >= 0 ? glyph[index][row] :
                            *s == '-' && row == 3 ? 31 : *s == '.' && row == 6 ? 4 :
                            *s == ':' && (row == 2 || row == 5) ? 4 :
                            *s == '%' ? percent[row] : 0;
            for (int col = 0; col < 5 && count < 4096; ++col)
                if (bits & (1u << (4 - col))) pixels[count++] = (SDL_Rect){x + col * scale, y + row * scale, scale, scale};
        }
    }
    SDL_RenderFillRects(r, pixels, count);
}
static void color(SDL_Renderer *r, int red, int green, int blue) { SDL_SetRenderDrawColor(r, red, green, blue, 255); }

static SDL_GameController *open_pad(void) {
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
        if (SDL_IsGameController(i)) { SDL_GameController *p = SDL_GameControllerOpen(i); if (p) return p; }
    return NULL;
}
static void change(Settings *s, int selected, int direction) {
    if (selected < 1 || selected > 6) return;
    int i = selected - 1, *p = fields(s, i);
    int step = i == 2 || i == 3 ? 5 : i == 4 ? 2 : 1;
    int v = *p + step * direction;
    if (i == 0) v = *p == 2 ? 3 : 2;
    if (i == 1 || i == 5) v = !*p;
    if (valid(i, v)) *p = v;
}

/* Returns 1 to run probe, 0 to exit. */
static int menu(Settings *s, int smoke, const char *status) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS)) { fprintf(stderr, "%s\n", SDL_GetError()); return 0; }
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
    SDL_Window *w = SDL_CreateWindow("Desert Storm Port Setup", 0, 0, 640, 480, SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_Renderer *r = w ? SDL_CreateRenderer(w, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) : NULL;
    if (!r) { fprintf(stderr, "frontend=%s\n", SDL_GetError()); if (w) SDL_DestroyWindow(w); SDL_Quit(); return 0; }
    SDL_RendererInfo info; SDL_GetRendererInfo(r, &info); printf("launcher_renderer=%s\n", info.name);
    SDL_RenderSetLogicalSize(r, 640, 480);
    SDL_GameController *pad = open_pad();
    int selected = 0, done = 0, run = 0;
    Uint32 start = SDL_GetTicks(), repeat = 0;
    const char *message = status;
    while (!done) {
        SDL_Event e; int move = 0, adjust = 0, accept = 0;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) done = 1;
            if (e.type == SDL_CONTROLLERDEVICEADDED && !pad) pad = open_pad();
            if (e.type == SDL_CONTROLLERDEVICEREMOVED && pad && !SDL_GameControllerGetAttached(pad)) {
                SDL_GameControllerClose(pad); pad = open_pad();
            }
            if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) move = -1;
                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) move = 1;
                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) adjust = -1;
                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) adjust = 1;
                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_A) accept = 1;
                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_B) done = 1;
            }
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_UP: move = -1; break; case SDLK_DOWN: move = 1; break;
                    case SDLK_LEFT: adjust = -1; break; case SDLK_RIGHT: adjust = 1; break;
                    case SDLK_RETURN: accept = 1; break; case SDLK_ESCAPE: done = 1; break;
                }
            }
        }
        if (pad && SDL_GetTicks() >= repeat) {
            Sint16 x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
            Sint16 y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
            if (y < -16000) move = -1; else if (y > 16000) move = 1;
            else if (x < -16000) adjust = -1; else if (x > 16000) adjust = 1;
            if (move || adjust) repeat = SDL_GetTicks() + 180;
        }
        selected = (selected + move + 9) % 9;
        if (adjust) change(s, selected, adjust);
        if (accept) {
            if (selected == 0) { if (save(s)) { done = 1; run = 1; } else message = "SAVE FAILED"; }
            else if (selected <= 6) change(s, selected, 1);
            else if (selected == 7) message = save(s) ? "SETTINGS SAVED" : "SAVE FAILED";
            else done = 1;
        }
        color(r, 21, 25, 25); SDL_RenderClear(r);
        color(r, 226, 191, 119); text(r, 30, 24, 3, "DESERT STORM");
        color(r, 151, 163, 157); text(r, 32, 58, 2, "PORTMASTER SETUP - BRINGUP BUILD");
        color(r, 227, 150, 104); text(r, 32, 90, 2, "GAME RUNTIME NOT BUILT");
        const char *labels[] = {"RUN HARDWARE CHECK", "OPENGL ES", "VSYNC", "STICK DEADZONE", "TRIGGER THRESHOLD", "CHECK SECONDS", "CHECK AUDIO", "SAVE SETTINGS", "EXIT"};
        for (int i = 0; i < 9; ++i) {
            int y = 128 + i * 30;
            if (i == selected) { color(r, 61, 72, 63); SDL_Rect box = {24,y-7,592,28}; SDL_RenderFillRect(r, &box); }
            color(r, i == selected ? 247 : 182, i == selected ? 219 : 189, i == selected ? 159 : 183);
            text(r, 36, y, 2, labels[i]);
            if (i >= 1 && i <= 6) {
                int n = *fields(s, i - 1); char value[24];
                if (i == 2 || i == 6) snprintf(value, sizeof value, "%s", n ? "ON" : "OFF");
                else snprintf(value, sizeof value, "%d%s", n, i == 3 || i == 4 ? "%" : "");
                text(r, 510, y, 2, value);
            }
        }
        color(r, 151, 163, 157); text(r, 32, 415, 1, "DPAD OR STICK: MOVE    LEFT-RIGHT: CHANGE    A: SELECT    B: EXIT");
        text(r, 32, 438, 1, message);
        if (!pad) text(r, 32, 456, 1, "NO CONTROLLER - CONNECT A PAD");
        if (smoke && SDL_GetTicks() - start > 1500) {
            int width, height; SDL_GetRendererOutputSize(r, &width, &height);
            SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
            if (surface) {
                if (!SDL_RenderReadPixels(r, NULL, surface->format->format, surface->pixels, surface->pitch))
                    printf("launcher_screenshot=%d\n", SDL_SaveBMP(surface, "launcher-smoke.bmp"));
                SDL_FreeSurface(surface);
            }
            done = 1;
        }
        SDL_RenderPresent(r);
    }
    if (pad) SDL_GameControllerClose(pad);
    SDL_DestroyRenderer(r); SDL_DestroyWindow(w); SDL_Quit(); return run;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (ds_paths_init()) { perror("initialize executable-relative storage"); return 1; }
    if (argc == 2 && !strcmp(argv[1], "--paths-check")) {
        printf("runtime_root=%s\nsave_dir=%s\n", getenv("DS_ROOT"), getenv("DS_SAVE_DIR"));
        return 0;
    }
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    Settings s; load(&s);
    const char *status = "640 X 480 - CONTROLLER SETTINGS APPLY TO THE HARDWARE CHECK";
    while (menu(&s, smoke, status)) {
        char es[8], seconds[8], deadzone[24], trigger[24];
        snprintf(es, sizeof es, "%d", s.es); snprintf(seconds, sizeof seconds, "%d", s.seconds);
        snprintf(deadzone, sizeof deadzone, "%.2f", s.deadzone / 100.0);
        snprintf(trigger, sizeof trigger, "%.2f", s.trigger / 100.0);
        SDL_setenv("DS_PAD_DEADZONE", deadzone, 1); SDL_setenv("PROBE_DEADZONE", deadzone, 1);
        SDL_setenv("DS_PAD_TRIGGER_THRESHOLD", trigger, 1);
        SDL_setenv("PROBE_VSYNC", s.vsync ? "1" : "0", 1);
        pid_t child = fork();
        if (child == 0) {
            char *args[] = {"./platform-probe.aarch64", "--es", es, "--seconds", seconds, "--d3d8-check", s.audio ? "--audio" : NULL, NULL};
            execv(args[0], args); perror("exec diagnostic"); _exit(127);
        }
        int result = -1;
        if (child > 0) waitpid(child, &result, 0);
        status = result == 0 ? "HARDWARE CHECK PASSED - GAME NOT YET AVAILABLE" : "HARDWARE CHECK FAILED - SEE LOG.TXT";
    }
    return 0;
}
