#include "settings.h"
#include "config.h"

#include <Preferences.h>

Settings g_settings;
static Preferences prefs;

static int clampDur(int s) {
    if (s < DURATION_MIN_SEC) s = DURATION_MIN_SEC;
    if (s > DURATION_MAX_SEC) s = DURATION_MAX_SEC;
    return s;
}

static uint8_t clampBri(int b) {
    if (b < BRIGHTNESS_MIN) b = BRIGHTNESS_MIN;
    if (b > BRIGHTNESS_MAX) b = BRIGHTNESS_MAX;
    return (uint8_t)b;
}

static uint16_t clampSpeed(int s) {
    if (s < ANIM_SPEED_MIN) s = ANIM_SPEED_MIN;
    if (s > ANIM_SPEED_MAX) s = ANIM_SPEED_MAX;
    return (uint16_t)s;
}

void settingsLoad() {
    prefs.begin("pokedex", false);   // stays open for the process lifetime
    g_settings.durationSec = clampDur(prefs.getInt("dur", DEFAULT_DURATION_SEC));
    g_settings.randomOrder = prefs.getBool("rand", true);            // random by default
    g_settings.animMode    = prefs.getUChar("amode", ANIM_MODE_FULL);
    if (g_settings.animMode > ANIM_MODE_STATIC) g_settings.animMode = ANIM_MODE_FULL;
    g_settings.brightness  = clampBri(prefs.getUChar("bri", PANEL_BRIGHTNESS));
    g_settings.speedPct    = clampSpeed(prefs.getUShort("spd", ANIM_SPEED_DEFAULT));
}

void settingsSave() {
    g_settings.durationSec = clampDur(g_settings.durationSec);
    g_settings.brightness  = clampBri(g_settings.brightness);
    g_settings.speedPct    = clampSpeed(g_settings.speedPct);
    prefs.putInt("dur", g_settings.durationSec);
    prefs.putBool("rand", g_settings.randomOrder);
    prefs.putUChar("amode", g_settings.animMode);
    prefs.putUChar("bri", g_settings.brightness);
    prefs.putUShort("spd", g_settings.speedPct);
}
