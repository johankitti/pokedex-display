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

// Prefetched next slide: its sprite bytes are fetched over the network while the
// current slide is still on screen, so the swap only costs a decode+draw and the
// slide timing tracks the duration clock instead of duration + fetch time.
static uint8_t* pendBuf   = nullptr;
static size_t   pendLen   = 0;
static char     pendName[24] = "";
static int      pendId    = -1;
static bool     pendReady = false;

static int clampDuration(int s) {
    if (s < DURATION_MIN_SEC) s = DURATION_MIN_SEC;
    if (s > DURATION_MAX_SEC) s = DURATION_MAX_SEC;
    return s;
}

// Blocking fetch of the next random sprite into the pending slot. Called while the
// current sprite is displayed, so its network latency hides inside the slide.
static void prefetchNext() {
    if (pendReady) return;
    uint8_t* buf = nullptr; size_t len = 0; int id = -1; char nm[24];
    if (pokemonPrefetch(nm, sizeof(nm), &id, &buf, &len)) {
        pendBuf = buf; pendLen = len; pendId = id;
        strncpy(pendName, nm, sizeof(pendName) - 1);
        pendName[sizeof(pendName) - 1] = 0;
        pendReady = true;
    }
}

// Swap to the prefetched slide (cheap decode+draw), then immediately start
// fetching the following one so it's ready before this slide's duration elapses.
static void advanceSlide() {
    if (!pendReady) prefetchNext();          // slow net / button-mash: fetch now
    bool ok = pendReady && displayDrawSprite(pendBuf, pendLen, pendName, pendId);
    if (pendBuf) { netFree(pendBuf); pendBuf = nullptr; }
    pendReady = false;

    if (ok) {
        curId = pendId;
        strncpy(curName, pendName, sizeof(curName) - 1);
        curName[sizeof(curName) - 1] = 0;
        nameScrolls = displayNameScrolls(curName);
        displayDrawName(curName);
    } else {
        displayStatus("no net");             // fetch/decode failed — retry via loop
        curId = -1;
    }
    slideStartMs = millis();
    prefetchNext();                          // fetch the next one during this slide
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

    advanceSlide();   // fetch (loading anim covers it) + show first, prefetch second
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
        advanceSlide();
    } else if (curId < 0) {
        if (millis() - slideStartMs >= 5000) advanceSlide();   // retry after a dropout
    } else if (millis() - slideStartMs >= (uint32_t)durationSec * 1000UL) {
        advanceSlide();
    } else if (nameScrolls) {
        displayDrawName(curName);   // animate marquee for long names
    }

    delay(10);
}
