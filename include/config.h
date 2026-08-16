#pragma once
// -----------------------------------------------------------------------------
// Pokédex Display — build-time configuration
// Pin map + panel constants come from docs/hardware-reference.md (Sections 2-3).
// -----------------------------------------------------------------------------

// ---- HUB75 panel geometry ----------------------------------------------------
#define PANEL_WIDTH      64
#define PANEL_HEIGHT     64
#define PANEL_CHAIN      1

// Brightness 0-255. ~20 is safe on USB power while testing; ~90 is comfortable
// indoors once running off the dedicated 5V supply.
#define PANEL_BRIGHTNESS 20

// ---- HUB75 -> ESP32-S3-Zero pin map (hardware-reference §3) -------------------
#define PIN_R1   1
#define PIN_G1   2
#define PIN_B1   3
#define PIN_R2   4
#define PIN_G2   5
#define PIN_B2   6
#define PIN_A    7
#define PIN_B    8
#define PIN_C    9
#define PIN_D    10
#define PIN_E    11   // needed: 64-row panel is 1/32 scan (HUB75E)
#define PIN_CLK  12
#define PIN_LAT  13
#define PIN_OE   43   // the top-right "TX" pad (UART0 free; logs go over USB)

// ---- Rotary encoder (electrokit art. 41021049, 24 detents + push) ------------
// HUB75 consumes GPIO1-13 + 43, so only GPIO44 is free on the side rows; the
// encoder's second signal lands on a bottom-edge pin (see hardware-reference §2).
// The encoder is a passive mechanical switch: common pin -> GND, internal
// pull-ups on A/B/SW, no VCC required.
#define PIN_ENC_A   44   // CLK  (free side-row pad, was "RX")
#define PIN_ENC_B   14   // DT   (bottom edge — back-side solder)
#define PIN_ENC_SW  0    // push: onboard BOOT button (no extra wire). Set to the
                         // encoder's own SW pin (e.g. 15) if you wire it instead.

// A 24-detent encoder driven full-quadrature yields ~4 counts per physical click.
// Tune if one detent doesn't map cleanly to one step.
#define ENC_COUNTS_PER_DETENT 4

// ---- Slideshow behaviour -----------------------------------------------------
#define DEX_MIN               1     // national dex range to draw random ids from
#define DEX_MAX               1025
#define DEFAULT_DURATION_SEC  60    // seconds each Pokémon is shown
#define DURATION_MIN_SEC      1
#define DURATION_MAX_SEC      300
#define ADJUST_IDLE_MS        2000  // idle after last scroll before saving + resuming

// ---- Sprite + name layout on the 64x64 panel ---------------------------------
// Default GFX font @ size 1: glyph 5x7 in a 6x8 cell -> 6px advance/char, 8px
// pitch between lines (7px visible + 1px). The name is pinned to the bottom rows
// (one or two lines with a 1px gap) and the sprite fills the rest above it. The
// exact split is computed at draw time from the name (see display.cpp), so a
// short name gets a taller sprite than a wrapped one.
#define TEXT_CHAR_W         6
#define TEXT_LINE_H         8   // vertical pitch between the two name lines
#define TEXT_GLYPH_H        7   // visible glyph height
#define NAME_MAX_LINES      2
#define NAME_CHARS_PER_LINE (PANEL_WIDTH / TEXT_CHAR_W)   // 10
#define LAYOUT_GAP          1   // minimal gap between sprite and name

// ---- Networking --------------------------------------------------------------
// Classic front pixel sprite (96x96 RGBA) from the PokéAPI sprites CDN.
#define SPRITE_URL_FMT "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/%d.png"
#define HTTP_TIMEOUT_MS   8000
#define FETCH_MAX_RETRIES 4        // try up to N different Pokémon before giving up

// ---- Wi-Fi setup portal ------------------------------------------------------
// Credentials are provisioned via WiFiManager's captive portal (stored in NVS),
// not compiled in. Join this AP during setup to configure your network.
#define AP_SETUP_SSID   "Pokedex-Setup"
#define SETUP_WINDOW_MS 3000       // press BOOT within this window at boot to force setup
