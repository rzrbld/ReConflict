#ifndef DS_CONFIGURATION_H
#define DS_CONFIGURATION_H
#include <stdint.h>
#define DS_GRAPHICS_COUNT 11
#define DS_GRAPHICS_REGISTRY_KEY "SOFTWARE\\Pivotal Games\\Conflict Desert Storm\\Device Settings\\"
typedef struct { const char *key,*label; uint32_t initial,maximum; int editable; } DSGraphicsOption;
extern const DSGraphicsOption ds_graphics_options[DS_GRAPHICS_COUNT];
typedef struct { uint32_t value[DS_GRAPHICS_COUNT],present; } DSGraphicsConfig;
const char *ds_configuration_value_label(int index,uint32_t value);
void ds_configuration_defaults(DSGraphicsConfig *config);
/* Missing file returns0, valid file1, invalid/I/O failure -1. Invalid loads
 * never change the caller's settings. File always lives under DS_ROOT/config. */
int ds_configuration_load(DSGraphicsConfig *config);
int ds_configuration_save(const DSGraphicsConfig *config);
/* Run before the game/host starts SDL. Returns after save/cancel, owns no
 * guest state and changes no registry. Next game boot applies explicit keys. */
int ds_configuration_main(void);
#endif
