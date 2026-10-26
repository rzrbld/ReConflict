#ifndef DS_D3D8_RENDERER_H
#define DS_D3D8_RENDERER_H
#include <GLES2/gl2.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint32_t rs[256], tss[8][32], viewport[6];
    float transform[512][16], material[17], light[8][26];
    uint32_t light_on[8], fvf;
    uint32_t texture_flipy[2];
} D8State;
typedef struct {
    GLint tex[2],has[2],cop[2],aop[2];
    GLint factor,afunc,aref,fogmode,fogcolor,fogparam,specular;
    GLint world,view,projection,normalmat,texmat0,texmat1,viewport,zrange;
    GLint flags,sources,texflags,material,power,ambient,eye,camera,localviewer,lights,enabled;
} D8UniformLocations;
typedef struct {
    uint32_t valid,light_on[8],specular,ambient;
    float matrix[5][16],light[8][26],material[17];
} D8GpuUploadCache;
typedef struct {
    uint64_t vertex_bytes,index_bytes,prefix_vertex_bytes,avoidable_prefix_bytes;
    uint64_t vertex_uploads,index_uploads,trimmed_draws,native_vertex_draws,native_index_draws;
} D8UploadStats;
typedef struct {
    uint32_t valid;
    union { GLint i[4]; GLfloat f[4]; } value[19];
} D8SmallUniformCache;
typedef struct {
    uint32_t valid,depth[3],blend[4],cull,stencil[8],color,bias,viewport[5],depth_range[2];
} D8RasterCache;
#define D8_TEXTURE_PARAMETER_SLOTS 1024
typedef struct { GLuint texture; uint32_t epoch,valid; GLint parameter[4]; } D8TextureParameters;
typedef struct {
    uint32_t enable_valid,enabled,pointer_valid,constant_valid;
    struct { GLuint buffer; GLint size; GLenum type; GLboolean normalized; GLsizei stride; uintptr_t offset; } pointer[6];
    GLfloat constant[6][4];
} D8AttributeCache;
typedef struct { D8AttributeCache attributes; D8TextureParameters texture_parameters[D8_TEXTURE_PARAMETER_SLOTS]; uint32_t texture_epoch; D8RasterCache raster; D8SmallUniformCache small_uniforms,gpu_small_uniforms; D8UploadStats uploads; int trim_vertex_prefix; void *rebased_indices; size_t rebased_capacity; D8GpuUploadCache gpu_uploaded; D8UniformLocations uniforms, gpu_uniforms; GLuint program, vbo, gpu_program, gpu_vbo, gpu_ibo; void *vertices; size_t capacity; } D8Renderer;
/* Optional facade-owned GL buffers. Offsets identify the CPU data/index
 * pointers' corresponding positions in the complete native buffer objects. */
typedef struct {
    GLuint vertex,index;
    size_t vertex_offset,index_offset,vertex_bytes,index_bytes;
    /* True only when the caller knows the CURRENT native framebuffer is0.
     * Unknown/offscreen targets retain inactive sampler clearing to prevent
     * texture/FBO feedback even with conditionally disabled sampling. */
    int default_framebuffer;
} D8NativeBuffers;
void d8_state_defaults(D8State *s, unsigned width, unsigned height);
int d8_renderer_init(D8Renderer *r);
void d8_renderer_close(D8Renderer *r);
/* Call after external GL raster mutations, including D3D Clear masks. */
void d8_renderer_invalidate_raster(D8Renderer *r);
void d8_renderer_invalidate_attributes(D8Renderer *r);
void d8_renderer_invalidate_texture_parameters(D8Renderer *r);
/* Call after texture creation/default setup and before deletion or external
 * parameter changes to this ID. Names may be reused by the GL implementation. */
void d8_renderer_forget_texture(D8Renderer *r, GLuint texture);
/* Actual glBufferData bytes since the previous call; read/reset together. */
D8UploadStats d8_renderer_take_upload_stats(D8Renderer *r);
int d8_renderer_draw(D8Renderer *r, const D8State *s, unsigned target_height,
    unsigned primitive, unsigned primitive_count, const void *vertices,
    size_t stride, unsigned vertex_count, const void *indices, unsigned index_size,
    unsigned base_vertex, const GLuint textures[2]);
int d8_renderer_draw_native(D8Renderer *r, const D8State *s, unsigned target_height,
    unsigned primitive, unsigned primitive_count, const void *vertices,
    size_t stride, unsigned vertex_count, const void *indices, unsigned index_size,
    unsigned base_vertex, const GLuint textures[2], const D8NativeBuffers *native);
/* Account facade BufferData/SubData calls; bytes are submitted CPU bytes. */
void d8_renderer_note_buffer_upload(D8Renderer *r, int index, size_t bytes);
const char *d8_renderer_error(void);
unsigned d8_primitive_vertices(unsigned type, unsigned count);
unsigned d8_fvf_stride(uint32_t fvf);
void d8_matrix_multiply(float out[16], const float a[16], const float b[16]);
#endif
