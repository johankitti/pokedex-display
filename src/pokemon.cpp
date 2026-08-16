#include "pokemon.h"
#include "config.h"
#include "net.h"
#include "display.h"
#include "names.h"

#include <pgmspace.h>
#include <esp_random.h>
#include <ctype.h>

const char* pokemonName(int id) {
    static char buf[24];
    if (id < 1 || id > DEX_NAME_COUNT) {
        strncpy(buf, "???", sizeof(buf));
        return buf;
    }
    PGM_P p = (PGM_P)pgm_read_ptr(&DEX_NAMES[id - 1]);
    strncpy_P(buf, p, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    for (char* c = buf; *c; ++c) *c = toupper((unsigned char)*c);   // panel shows NAMES uppercase
    return buf;
}

static int randomId() {
    uint32_t span = (uint32_t)(DEX_MAX - DEX_MIN + 1);
    return DEX_MIN + (int)(esp_random() % span);
}

bool pokemonPrefetch(char* outName, size_t nameLen, int* outId,
                     uint8_t** outBuf, size_t* outLen) {
    char url[160];
    for (int attempt = 0; attempt < FETCH_MAX_RETRIES; attempt++) {
        int id = randomId();
        const char* nm = pokemonName(id);   // captured now so the layout can size
                                            // itself for a 1- or 2-line name later
        snprintf(url, sizeof(url), SPRITE_URL_FMT, id);

        uint8_t* buf = nullptr;
        size_t len = 0;
        if (httpGetBinary(url, &buf, &len)) {
            strncpy(outName, nm, nameLen - 1);
            outName[nameLen - 1] = 0;
            *outId = id;
            *outBuf = buf;
            *outLen = len;
            Serial.printf("[poke] prefetch #%d %s (%u bytes)\n", id, nm, (unsigned)len);
            return true;
        }
        netFree(buf);
        Serial.printf("[poke] fetch failed id=%d (attempt %d)\n", id, attempt + 1);
        delay(200);
    }
    return false;
}
