#pragma once
#include <Arduino.h>

// Initialise the HUB75 panel (pins/geometry from config.h).
void displayInit();

// Blank the whole panel.
void displayClear();

// State 1: animated "Loading" / "Loading." / ... centered on screen, driven by a
// background task so it keeps moving during blocking network fetches. Any sprite
// or status draw stops it automatically; displayLoadingStop() is idempotent.
void displayLoadingStart();
void displayLoadingStop();

// Decode a PNG held in RAM and draw it scaled + centered above the name. The
// sprite height depends on how many rows `name` needs (1 or 2), so pass the same
// name you'll hand to displayDrawName(). `id` is drawn as a "#NNN" label in the
// top-left corner. Clears the sprite region only. Returns false if the PNG can't
// be decoded.
bool displayDrawSprite(const uint8_t* png, size_t len, const char* name, int id);

// Draw `name` in the bottom strip. Centered if it fits, else marquee-scrolled
// (call every frame to animate). Clears the strip first.
void displayDrawName(const char* name);

// True if `name` is too wide to fit and will marquee-scroll.
bool displayNameScrolls(const char* name);

// Big seconds value in the bottom strip, shown while adjusting the duration.
void displayDrawDuration(int seconds);

// State 0: Wi-Fi setup screen shown while the captive portal is open — the AP
// name to join and the portal address.
void displaySetup(const char* ssid, const char* ip);

// Short status message centered on a blank screen (boot / errors).
void displayStatus(const char* msg);

void displaySetBrightness(uint8_t b);
