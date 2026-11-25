/* voiptest: library version and layer, then a silent audio check (opens the voice output and input). */
#include "voip.h"
#include <stdio.h>
int main(void) {
    char r[128];
    printf("libtgvoip %s, layer %d\n", gv_version(), gv_max_layer());
    int rc = gv_audio_check(400, r, sizeof r);
    printf("audio: %s (%d)\n", r, rc);
    return rc ? 1 : 0;
}
