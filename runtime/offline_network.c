#include "offline_network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int ds_offline_skip_directplay_install_probe(void) {
    const char *mode=getenv("DS_NETWORK_MODE");
    int offline=!mode || !strcmp(mode,"offline") || !strcmp(mode,"singleplayer");
    static int reported;
    if(offline && !reported) {
        fprintf(stderr,"[offline] Skipping legacy dpnhpast.dll installation-presence gate for single-player. DirectPlay/NAT networking is unavailable.\n");
        reported=1;
    }
    return offline;
}
