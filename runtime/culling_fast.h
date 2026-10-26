#ifndef DS_CULLING_FAST_H
#define DS_CULLING_FAST_H
/* Framed ESP/ESI at original0x50C359.0 unchanged fallback;1 accept0x50C40F;
 *2 reject0x50C48A. Caller imports flags before resuming original labels. */
int ds_culling_prefix_50c359(void);
void ds_culling_fast_control(int enable);
#endif
