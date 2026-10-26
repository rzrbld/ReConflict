#ifndef DS_WIN32_EXTRA_H
#define DS_WIN32_EXTRA_H
#include "win32hle.h"
extern const win32hle_shim ds_win32_extra_shims[];
/* Populate the original game's registry with verified startup settings. */
void ds_seed_game_settings(void);
/* Post the original launcher's OK command after its actual WM_INITDIALOG.
 * Other dialogs are left alone; original mode-selection validation still runs. */
void ds_startup_dialog_hook(uint32_t hwnd, uint32_t resource_id);
#endif
