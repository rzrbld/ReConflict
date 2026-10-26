#ifndef DS_D3D8_LIGHTS_H
#define DS_D3D8_LIGHTS_H
#include <stdint.h>
#define DS_D8_LOGICAL_LIGHTS 256u
#define DS_D8_ACTIVE_LIGHTS 8u
#define DS_D8_LIGHT_WORDS 26u
/* Explicit title limit: original engine allocates254 IDs. This is not an
 * assertion that the Direct3D8 API generally restricts IDs to255. Type is a
 * DWORD in the first4 bytes, followed by the original D3DLIGHT8 float fields. */
typedef struct {
    float light[DS_D8_LOGICAL_LIGHTS][DS_D8_LIGHT_WORDS];
    uint8_t defined[DS_D8_LOGICAL_LIGHTS],enabled[DS_D8_LOGICAL_LIGHTS];
    uint32_t active_count;
} D8LightBank;
typedef struct {
    uint8_t definition[DS_D8_LOGICAL_LIGHTS],enable[DS_D8_LOGICAL_LIGHTS];
} D8LightMask;
void ds_d8_lights_init(D8LightBank *bank);
/*0 success,-1 invalid ID/pointer/type/capacity; failures change no bank/output.
 * Definitions default disabled. Set preserves existing enabled state.
 * Enable of an undefined ID creates the documented default directional light,
 * even when disabling, unless operation fails the8-active limit. */
int ds_d8_lights_set(D8LightBank *bank,uint32_t id,const float light[26]);
int ds_d8_lights_get(const D8LightBank *bank,uint32_t id,float light[26]);
int ds_d8_lights_enable(D8LightBank *bank,uint32_t id,int enable);
int ds_d8_lights_get_enable(const D8LightBank *bank,uint32_t id,uint32_t *enable);
/* Compact enabled IDs in ascending order, zero all unused hardware slots.
 * IDs output optional; unused IDs become UINT32_MAX. Returns active count. */
unsigned ds_d8_lights_pack(const D8LightBank *bank,float light[8][26],uint32_t enabled[8],uint32_t ids[8]);
void ds_d8_light_mask_clear(D8LightMask *mask);
void ds_d8_light_mask_all(D8LightMask *mask);
/* Source is a complete valid captured bank; only selected fields apply.
 * Enable-only restores keep current definitions; if missing, synthesize the
 * API default, not the source definition. Whole operation validates final
 * active count transactionally, permitting a full8-light swap without a
 * transient ninth-light failure. Definition-only removal of an active light
 * is rejected. All masks can exactly restore undefined slots as well. */
int ds_d8_lights_apply_masked(D8LightBank *bank,const D8LightBank *source,const D8LightMask *mask);
#endif
