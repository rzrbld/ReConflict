/* Derived from ConflictDesertStormPatch, Copyright (c) 2026 eSKylezZ (MIT).
 * Retains its PS2 button numbering, +/-128 axes, deadzone rescaling, and
 * neutral POV sentinels. Replaces XInput/Win32 HID reads with SDL2.
 * No game-memory writes or x86 detours are installed by this adapter. */
#include "desert_pad.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

_Static_assert(sizeof(DS_PadState) == 176, "guest joystick prefix ABI");
_Static_assert(offsetof(DS_PadState, buttons) == 48, "guest button offset");
enum { TRIGGER_LEFT = 100, TRIGGER_RIGHT = 101 };

void ds_pad_defaults(DS_PadConfig *c) {
    *c = (DS_PadConfig){.deadzone = 0.22f, .trigger_threshold = 0.15f,
        .button = {SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_B,
                   SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_X,
                   TRIGGER_LEFT, TRIGGER_RIGHT,
                   SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
                   SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_LEFTSTICK,
                   SDL_CONTROLLER_BUTTON_RIGHTSTICK, SDL_CONTROLLER_BUTTON_START,
                   SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
                   SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT}};
}

int ds_pad_load(DS_PadConfig *c, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    DS_PadConfig candidate = *c;
    char line[160], name[80]; int index, ok = 1;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        float v;
        if (sscanf(line, "deadzone=%f", &v) == 1) {
            if (!isfinite(v) || v < 0 || v >= 1) { ok = 0; break; }
            candidate.deadzone = v; continue;
        }
        if (sscanf(line, "trigger_threshold=%f", &v) == 1) {
            if (!isfinite(v) || v < 0 || v >= 1) { ok = 0; break; }
            candidate.trigger_threshold = v; continue;
        }
        if (sscanf(line, "%d=%79s", &index, name) != 2 || index < 0 || index >= 16) { ok = 0; break; }
        int button = !strcmp(name, "lefttrigger") ? TRIGGER_LEFT :
                     !strcmp(name, "righttrigger") ? TRIGGER_RIGHT :
                     (int)SDL_GameControllerGetButtonFromString(name);
        if (button == SDL_CONTROLLER_BUTTON_INVALID) { ok = 0; break; }
        candidate.button[index] = button;
    }
    if (ferror(f)) ok = 0;
    fclose(f);
    if (!ok) return -1;
    *c = candidate;
    return 0;
}

static int32_t scale(Sint16 value, float dz) {
    float v = value / 32767.0f, mag = fabsf(v);
    if (mag <= dz) return 0;
    v = (v < 0 ? -1 : 1) * fminf(1, (mag - dz) / (1 - dz));
    return (int32_t)(v * 128);
}

void ds_pad_encode(const DS_PadRaw *raw, const DS_PadConfig *c, DS_PadState *out) {
    memset(out, 0, sizeof(*out));
    for (int i = 0; i < 4; ++i) out->pov[i] = UINT32_MAX;
    if (!raw) return;
    out->x = scale(raw->axes[SDL_CONTROLLER_AXIS_LEFTX], c->deadzone);
    out->y = scale(raw->axes[SDL_CONTROLLER_AXIS_LEFTY], c->deadzone);
    out->z = scale(raw->axes[SDL_CONTROLLER_AXIS_RIGHTX], c->deadzone);
    out->rz = scale(raw->axes[SDL_CONTROLLER_AXIS_RIGHTY], c->deadzone);
    for (int i = 0; i < 16; ++i) {
        int button = c->button[i];
        int down = button == TRIGGER_LEFT ? raw->axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT] > 32767 * c->trigger_threshold :
                   button == TRIGGER_RIGHT ? raw->axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] > 32767 * c->trigger_threshold :
                   (raw->buttons & (1u << button)) != 0;
        out->buttons[i] = down ? 0x80 : 0;
    }
}

void ds_pad_sample(SDL_GameController *pad, const DS_PadConfig *c, DS_PadState *out) {
    DS_PadRaw raw = {{0}, 0};
    if (!pad || !SDL_GameControllerGetAttached(pad)) { ds_pad_encode(NULL, c, out); return; }
    for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; ++a)
        raw.axes[a] = SDL_GameControllerGetAxis(pad, (SDL_GameControllerAxis)a);
    for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; ++b)
        if (SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)b)) raw.buttons |= 1u << b;
    ds_pad_encode(&raw, c, out);
}

int ds_pad_selftest(void) {
    DS_PadConfig c; ds_pad_defaults(&c);
    DS_PadRaw raw = {{0}, 0}; DS_PadState out;
    ds_pad_encode(NULL, &c, &out);
    if (out.x || out.y || out.z || out.rz || out.pov[0] != UINT32_MAX) return 0;
    raw.axes[SDL_CONTROLLER_AXIS_LEFTX] = -32768;
    raw.axes[SDL_CONTROLLER_AXIS_LEFTY] = 32767;
    raw.axes[SDL_CONTROLLER_AXIS_RIGHTX] = 100;
    raw.axes[SDL_CONTROLLER_AXIS_RIGHTY] = 32767;
    raw.axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT] = 32767;
    raw.buttons = (1u << SDL_CONTROLLER_BUTTON_A) | (1u << SDL_CONTROLLER_BUTTON_START) |
                  (1u << SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    ds_pad_encode(&raw, &c, &out);
    if (out.x != -128 || out.y != 128 || out.z != 0 || out.rz != 128 ||
        out.buttons[2] != 0x80 || out.buttons[4] != 0x80 || out.buttons[5] ||
        out.buttons[11] != 0x80 || out.buttons[15] != 0x80 || out.buttons[0]) return 0;
    raw.axes[SDL_CONTROLLER_AXIS_LEFTX] = 16384;
    ds_pad_encode(&raw, &c, &out);
    return out.x >= 45 && out.x <= 46;
}
