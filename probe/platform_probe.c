/* Hardware bring-up only. This program contains no translated game code. */
#include <SDL.h>
#include <GLES2/gl2.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "desert_pad.h"
#include "d3d8_gles.h"
#include "../runtime/paths.h"

static SDL_GameController *pad;
static SDL_JoystickID pad_id = -1;
static float deadzone = 0.22f;
static int callbacks;

static float setting(const char *name, float fallback, float low, float high) {
    const char *s = SDL_getenv(name);
    char *end;
    float v = s ? strtof(s, &end) : fallback;
    if (!s || end == s || *end || !isfinite(v) || v < low || v > high) return fallback;
    return v;
}

static void stick(Sint16 ix, Sint16 iy, float *x, float *y) {
    float fx = ix / 32768.0f, fy = iy / 32768.0f;
    float len = sqrtf(fx * fx + fy * fy);
    float scale = len <= deadzone ? 0 : (fminf(len, 1) - deadzone) / ((1 - deadzone) * len);
    *x = fx * scale; *y = fy * scale;
}

static void open_pad(void) {
    if (pad) return;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (!SDL_IsGameController(i)) continue;
        pad = SDL_GameControllerOpen(i);
        if (!pad) { printf("controller_open_failed=%s\n", SDL_GetError()); continue; }
        pad_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad));
        char *mapping = SDL_GameControllerMapping(pad);
        printf("controller name=%s instance=%d mapping=%s\n",
               SDL_GameControllerName(pad), pad_id, mapping ? mapping : "unknown");
        SDL_free(mapping);
        break;
    }
}

static GLuint shader(GLenum type, const char *source) {
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &source, NULL); glCompileShader(id);
    GLint ok; glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]; glGetShaderInfoLog(id, sizeof(log), NULL, log);
        fprintf(stderr, "shader_error=%s\n", log); glDeleteShader(id); return 0;
    }
    return id;
}

static GLuint program(void) {
    GLuint vs = shader(GL_VERTEX_SHADER, "attribute vec2 position; void main(){gl_Position=vec4(position,0.,1.);}");
    GLuint fs = shader(GL_FRAGMENT_SHADER, "precision mediump float; void main(){gl_FragColor=vec4(1.,0.,0.,1.);}");
    if (!vs || !fs) { if (vs) glDeleteShader(vs); if (fs) glDeleteShader(fs); return 0; }
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs); glBindAttribLocation(p, 0, "position");
    glLinkProgram(p); glDeleteShader(vs); glDeleteShader(fs);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048]; glGetProgramInfoLog(p, sizeof(log), NULL, log);
        fprintf(stderr, "link_error=%s\n", log); glDeleteProgram(p); return 0;
    }
    return p;
}

static void audio(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    /* Silence proves the callback path, without claiming audible output. */
    SDL_memset(stream, 0, (size_t)len); ++callbacks;
}

int main(int argc, char **argv) {
    if (ds_paths_init()) { perror("initialize executable-relative storage"); return 1; }
    if (argc == 2 && !strcmp(argv[1], "--paths-check")) {
        printf("runtime_root=%s\nsave_dir=%s\n", getenv("DS_ROOT"), getenv("DS_SAVE_DIR"));
        return 0;
    }
    int seconds = 8, want_audio = 0, want_es = 2, d3d8_check = 0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--seconds") && i + 1 < argc) seconds = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--es") && i + 1 < argc) want_es = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--audio")) want_audio = 1;
        else if (!strcmp(argv[i], "--d3d8-check")) d3d8_check = 1;
        else { fprintf(stderr, "Usage: %s [--seconds 1..600] [--es 2|3] [--audio] [--d3d8-check]\n", argv[0]); return 2; }
    }
    if (seconds < 1 || seconds > 600 || (want_es != 2 && want_es != 3)) return 2;
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (!ds_pad_selftest()) { fprintf(stderr, "guest_pad_selftest=FAIL\n"); return 1; }
    printf("guest_pad_selftest=PASS\n");
    DS_PadConfig pad_config; ds_pad_defaults(&pad_config);
    const char *pad_config_file = SDL_getenv("DS_PAD_CONFIG");
    if (pad_config_file && ds_pad_load(&pad_config, pad_config_file)) {
        fprintf(stderr, "Invalid controller config: %s\n", pad_config_file); return 2;
    }
    pad_config.deadzone = setting("DS_PAD_DEADZONE", pad_config.deadzone, 0, 0.95f);
    pad_config.trigger_threshold = setting("DS_PAD_TRIGGER_THRESHOLD", pad_config.trigger_threshold, 0, 0.95f);
    deadzone = setting("PROBE_DEADZONE", 0.22f, 0, 0.95f);
    Uint32 flags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS;
    if (want_audio) flags |= SDL_INIT_AUDIO;
    if (SDL_Init(flags) != 0) { fprintf(stderr, "SDL_Init=%s\n", SDL_GetError()); return 1; }
    SDL_version ver; SDL_GetVersion(&ver);
    printf("PLATFORM_PROBE_ONLY SDL=%d.%d.%d video=%s joysticks=%d deadzone=%.2f\n",
           ver.major, ver.minor, ver.patch, SDL_GetCurrentVideoDriver(), SDL_NumJoysticks(), deadzone);
    const char *db = SDL_getenv("PROBE_CONTROLLER_DB");
    if (db) printf("controller_db_entries=%d\n", SDL_GameControllerAddMappingsFromFile(db));
    /* Device-specific PortMaster mappings must win over the generic database. */
    const char *overrides = SDL_getenv("SDL_GAMECONTROLLERCONFIG");
    if (overrides) {
        char *copy = SDL_strdup(overrides), *line = copy;
        while (line && *line) {
            char *next = strchr(line, '\n');
            if (next) *next++ = '\0';
            if (*line) printf("controller_override=%d\n", SDL_GameControllerAddMapping(line));
            line = next;
        }
        SDL_free(copy);
    }
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        char guid[64]; SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), guid, sizeof(guid));
        printf("joystick index=%d name=%s guid=%s gamecontroller=%d\n", i,
               SDL_JoystickNameForIndex(i), guid, SDL_IsGameController(i));
    }
    open_pad();
    if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES) ||
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, want_es) ||
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0) ||
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1)) {
        fprintf(stderr, "context_attributes=%s\n", SDL_GetError()); SDL_Quit(); return 1;
    }
    SDL_Window *window = SDL_CreateWindow("Desert Storm platform diagnostic (no game)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480,
        SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_RESIZABLE);
    if (!window) { fprintf(stderr, "window=%s\n", SDL_GetError()); SDL_Quit(); return 1; }
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        fprintf(stderr, "context=%s\n", SDL_GetError()); SDL_DestroyWindow(window); SDL_Quit(); return 1;
    }
    SDL_GL_SetSwapInterval((int)setting("PROBE_VSYNC", 1, 0, 1));
    int major = 0, minor = 0, profile = 0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile);
    printf("GL_VERSION=%s\nGL_RENDERER=%s\nGL_VENDOR=%s\nGLSL=%s\ncontext=%d.%d profile=%d\n",
           glGetString(GL_VERSION), glGetString(GL_RENDERER), glGetString(GL_VENDOR),
           glGetString(GL_SHADING_LANGUAGE_VERSION), major, minor, profile);
    printf("GL_EXTENSIONS=%s\n", glGetString(GL_EXTENSIONS));
    GLuint p = program(), vbo = 0;
    int passed = p != 0, active = 1, running = 1;
    DS_Gles d3d = {0};
    GLuint argb_texture = 0, rgb565_texture = 0;
    if (d3d8_check) {
        uint32_t red_argb = 0xffff0000u; uint16_t red565 = 0xf800u;
        passed = passed && ds_gles_init(&d3d) == 0;
        argb_texture = ds_gles_texture(21, 1, 1, &red_argb, 4);
        rgb565_texture = ds_gles_texture(23, 1, 1, &red565, 2);
        passed = passed && argb_texture && rgb565_texture;
        printf("D3D8_GLES_check=XYZRHW_ARGB8_RGB565_XYZ\n");
    }
    const GLfloat triangle[] = {-1,-1, 1,-1, 0,1};
    glGenBuffers(1, &vbo); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(triangle), triangle, GL_STATIC_DRAW);
    SDL_AudioDeviceID sound = 0;
    int audio_ok = 1;
    if (want_audio) {
        SDL_AudioSpec desired, got;
        SDL_zero(desired); desired.freq = 48000; desired.format = AUDIO_S16SYS;
        desired.channels = 2; desired.samples = 512; desired.callback = audio;
        sound = SDL_OpenAudioDevice(NULL, 0, &desired, &got, SDL_AUDIO_ALLOW_ANY_CHANGE);
        if (!sound) { printf("audio_open_failed=%s\n", SDL_GetError()); audio_ok = 0; }
        else { printf("audio=%s hz=%d channels=%d format=%u\n", SDL_GetCurrentAudioDriver(), got.freq, got.channels, got.format); SDL_PauseAudioDevice(sound, 0); }
    }
    Uint32 started = SDL_GetTicks(), last = started;
    int frames = 0;
    while (running && passed && SDL_GetTicks() - started < (Uint32)seconds * 1000u) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            if (ev.type == SDL_APP_WILLENTERBACKGROUND) active = 0;
            if (ev.type == SDL_APP_DIDENTERFOREGROUND) active = 1;
            if (ev.type == SDL_RENDER_DEVICE_RESET) {
                fprintf(stderr, "context_lost: restart diagnostic to recreate resources\n"); passed = 0;
            }
            if (ev.type == SDL_CONTROLLERDEVICEADDED) open_pad();
            if (ev.type == SDL_CONTROLLERDEVICEREMOVED && ev.cdevice.which == pad_id) {
                printf("controller_removed instance=%d\n", pad_id);
                SDL_GameControllerClose(pad); pad = NULL; pad_id = -1; open_pad();
            }
            if (ev.type == SDL_CONTROLLERDEVICEREMAPPED) printf("controller_remapped instance=%d\n", ev.cdevice.which);
            if (ev.type == SDL_CONTROLLERBUTTONDOWN || ev.type == SDL_CONTROLLERBUTTONUP) {
                printf("button instance=%d name=%s down=%d\n", ev.cbutton.which,
                       SDL_GameControllerGetStringForButton((SDL_GameControllerButton)ev.cbutton.button),
                       ev.cbutton.state == SDL_PRESSED);
            }
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }
        if (pad && SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START) &&
                   SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK)) running = 0;
        if (!active) { SDL_Delay(20); continue; }
        int w, h; SDL_GL_GetDrawableSize(window, &w, &h);
        if (w < 1 || h < 1) { SDL_Delay(20); continue; }
        glViewport(0, 0, w, h); glClearColor(0, 0, 1, 1); glClear(GL_COLOR_BUFFER_BIT);
        if (d3d8_check) {
            struct { float pos[4]; uint32_t color; float uv[2]; } vertices[] = {
                {{-.5f,h-.5f,.5f,1},UINT32_MAX,{0,0}},
                {{w-.5f,h-.5f,.5f,1},UINT32_MAX,{0,0}},
                {{w*.5f-.5f,-.5f,.5f,1},UINT32_MAX,{0,0}}};
            if (frames == 2) {
                struct { float pos[3]; uint32_t color; } xyz[] = {
                    {{-1,-1,.5f},0xffff0000u},{{1,-1,.5f},0xffff0000u},{{0,1,.5f},0xffff0000u}};
                const float identity[16] = {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
                if (ds_gles_draw_up(&d3d,0x42,4,1,xyz,sizeof(xyz[0]),w,h,identity,0)) passed=0;
            } else if (ds_gles_draw_up(&d3d,0x144,4,1,vertices,sizeof(vertices[0]),w,h,NULL,
                                      frames==1?rgb565_texture:argb_texture)) passed=0;
        } else {
            glUseProgram(p); glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL); glEnableVertexAttribArray(0);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        if (frames < (d3d8_check ? 3 : 1)) {
            unsigned char center[4] = {0}, corner[4] = {0};
            glReadPixels(w / 2, h / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, center);
            glReadPixels(1, h - 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, corner);
            passed = passed && center[0] > 240 && center[1] < 15 && center[2] < 15 &&
                     corner[0] < 15 && corner[1] < 15 && corner[2] > 240;
            printf("framebuffer=%dx%d center=%u,%u,%u corner=%u,%u,%u readback=%s\n",
                w, h, center[0], center[1], center[2], corner[0], corner[1], corner[2], passed ? "PASS" : "FAIL");
        }
        GLenum err = glGetError();
        if (err) { fprintf(stderr, "GL_error=%#x\n", err); passed = 0; }
        SDL_GL_SwapWindow(window); ++frames;
        if (SDL_GetTicks() - last > 1000) {
            if (pad) {
                float x, y, rx, ry;
                stick(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX), SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY), &x, &y);
                stick(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX), SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY), &rx, &ry);
                float lt = fmaxf(0, SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.0f);
                float rt = fmaxf(0, SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.0f);
                printf("axes instance=%d move=%.3f,%.3f camera=%.3f,%.3f triggers=%.3f,%.3f\n", pad_id, x,y,rx,ry,lt,rt);
                DS_PadState state; ds_pad_sample(pad, &pad_config, &state);
                unsigned mask = 0;
                for (int b = 0; b < 16; ++b) if (state.buttons[b]) mask |= 1u << b;
                printf("guest_pad x=%d y=%d z=%d rz=%d buttons=%#x\n", state.x, state.y, state.z, state.rz, mask);
            }
            last = SDL_GetTicks();
        }
    }
    if (sound) { SDL_PauseAudioDevice(sound, 1); SDL_CloseAudioDevice(sound); if (!callbacks) passed = 0; }
    passed = passed && audio_ok;
    printf("RESULT=%s frames=%d elapsed_ms=%u audio_callbacks=%d controller_connected=%d\n",
           passed ? "PASS" : "FAIL", frames, SDL_GetTicks() - started, callbacks, pad != NULL);
    glDeleteBuffers(1, &vbo); glDeleteProgram(p);
    if (d3d8_check) {
        glDeleteTextures(1,&argb_texture); glDeleteTextures(1,&rgb565_texture); ds_gles_destroy(&d3d);
    }
    if (pad) SDL_GameControllerClose(pad);
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
    return passed ? 0 : 1;
}
