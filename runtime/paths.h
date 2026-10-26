#ifndef DS_PATHS_H
#define DS_PATHS_H

/* Linux/PortMaster: initialize before opening files or starting SDL.
 * Returns -1 on failure; errno describes the failure. Never falls back to cwd.
 */
int ds_paths_init(void);

#endif
