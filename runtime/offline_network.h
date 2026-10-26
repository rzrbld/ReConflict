#ifndef DS_OFFLINE_NETWORK_H
#define DS_OFFLINE_NETWORK_H
/* Explicit single-player compatibility policy. This does not provide NAT,
 * DirectPlay service providers or a network connection. */
int ds_offline_skip_directplay_install_probe(void);
#endif
