/* SDL2 adapter for the game-state layout documented by ConflictDesertStormPatch.
 * Adapted from padio.cpp / controller.cpp, Copyright (c) 2026 eSKylezZ.
 * MIT license: see ../third_party/ConflictDesertStormPatch/LICENSE. */
#ifndef DESERT_PAD_H
#define DESERT_PAD_H
#include <SDL.h>
#include <stdint.h>

/* Fixed-width Win32 fields: Linux AArch64 long is 64-bit, Win32 LONG is 32-bit. */
typedef struct {
    int32_t x, y, z, rx, ry, rz, slider[2];
    uint32_t pov[4];
    uint8_t buttons[128];
} DS_PadState;

typedef struct {
    Sint16 axes[SDL_CONTROLLER_AXIS_MAX];
    uint32_t buttons;
} DS_PadRaw;

typedef struct {
    float deadzone, trigger_threshold;
    int button[16];
} DS_PadConfig;

void ds_pad_defaults(DS_PadConfig *config);
int ds_pad_load(DS_PadConfig *config, const char *path);
void ds_pad_encode(const DS_PadRaw *raw, const DS_PadConfig *config, DS_PadState *out);
void ds_pad_sample(SDL_GameController *pad, const DS_PadConfig *config, DS_PadState *out);
int ds_pad_selftest(void);
#endif
