/* Initial D3D8 fixed-function subset. No guest COM facade is installed yet. */
#ifndef D3D8_GLES_H
#define D3D8_GLES_H
#include <GLES2/gl2.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    GLuint program, vbo;
    GLint viewport, transformed, matrix, textured;
    void *staging;
    size_t capacity;
} DS_Gles;
int ds_gles_init(DS_Gles *r);
void ds_gles_destroy(DS_Gles *r);
/* FVF XYZ or XYZRHW; optional DIFFUSE and one 2D texture coordinate set.
 * primitive: D3DPT_TRIANGLELIST=4, STRIP=5, FAN=6. Matrix uses D3D row-major
 * storage and row-vector semantics. Rejects unsupported formats explicitly. */
int ds_gles_draw_up(DS_Gles *r, unsigned fvf, unsigned primitive,
                    unsigned primitive_count, const void *data, size_t stride,
                    int width, int height, const float matrix[16], GLuint texture);
/* A8R8G8B8=21, X8R8G8B8=22, R5G6B5=23. Returns 0 on unsupported input. */
GLuint ds_gles_texture(unsigned format, unsigned width, unsigned height,
                       const void *pixels, size_t pitch);
#endif
