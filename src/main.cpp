#include <Arduino.h>
#include <Preferences.h>

#include "config.h"
#include "display.h"
#include "net.h"
#include "pokemon.h"
#include "input.h"

static Preferences prefs;
static int durationSec = DEFAULT_DURATION_SEC;

enum Mode { SLIDESHOW, ADJUSTING };
static Mode mode = SLIDESHOW;

static char curName[24] = "";
static int  curId = -1;
static bool nameScrolls = false;
static uint32_t slideStartMs = 0;
static uint32_t lastAdjustMs = 0;

static int clampDuration(int s) {
    if (s < DURATION_MIN_SEC) s = DURATION_MIN_SEC;
    if (s > DURATION_MAX_SEC) s = DURATION_MAX_SEC;
    return s;
}

// Fetch + draw the next random Pokémon. Keeps the previous image on screen while
// loading (drawSprite clears the sprite region only on success).
static void nextPokemon() {
    int id = pokemonNext(curName, sizeof(curName));
    if (id < 0) {
        displayStatus("no net");
        curId = -1;
        slideStartMs = millis();
        return;
    }
    curId = id;
    nameScrolls = displayNameScrolls(curName);
    displayDrawName(curName);
    slideStartMs = millis();
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n[boot] Pokedex Display");

    prefs.begin("pokedex", false);
    durationSec = clampDuration(prefs.getInt("dur", DEFAULT_DURATION_SEC));
    Serial.printf("[boot] duration=%ds\n", durationSec);

    displayInit();
    inputInit();

    // Setup window: press BOOT now to (re)configure Wi-Fi via the captive portal.
    // We poll here rather than at reset because BOOT is a strapping pin — holding
    // it during reset would enter the ROM download bootloader instead of our app.
    displayStatus("press BOOT\nfor wifi\nsetup");
    bool forcePortal = false;
    uint32_t t = millis();
    while (millis() - t < SETUP_WINDOW_MS) {
        if (digitalRead(PIN_ENC_SW) == LOW) { forcePortal = true; break; }
        delay(20);
    }

    displayStatus("Wi-Fi...");
    if (netStart(forcePortal)) {
        Serial.println("[net] connected");
        displayLoadingStart();          // State 1: animate while the first fetch runs
    } else {
        // Portal is kept open until configured, so this only trips on a hard
        // failure — reboot and retry, as WiFiManager recommends.
        Serial.println("[net] FAILED — restarting");
        displayStatus("wifi fail\nrestart");
        delay(1500);
        ESP.restart();
    }

    nextPokemon();
}

void loop() {
    int  delta = inputReadDelta();
    bool btn   = inputButtonPressed();

    // Any knob movement enters adjust mode and nudges the duration.
    if (delta != 0) {
        durationSec = clampDuration(durationSec + delta);
        mode = ADJUSTING;
        lastAdjustMs = millis();
        displayDrawDuration(durationSec);
    }

    if (mode == ADJUSTING) {
        if (millis() - lastAdjustMs > ADJUST_IDLE_MS) {
            prefs.putInt("dur", durationSec);
            Serial.printf("[main] duration=%ds saved\n", durationSec);
            mode = SLIDESHOW;
            if (curId > 0) displayDrawName(curName);   // restore name strip
            slideStartMs = millis();                   // restart timing
        }
        delay(5);
        return;
    }

    // SLIDESHOW
    if (btn) {
        nextPokemon();
    } else if (curId < 0) {
        if (millis() - slideStartMs >= 5000) nextPokemon();   // retry after a dropout
    } else if (millis() - slideStartMs >= (uint32_t)durationSec * 1000UL) {
        nextPokemon();
    } else if (nameScrolls) {
        displayDrawName(curName);   // animate marquee for long names
    }

    delay(10);
}
