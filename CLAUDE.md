# Pokédex Display — project notes for agents

Firmware for an ESP32-S3-Zero driving a 64×64 HUB75 RGB LED panel. It shows a
random Pokémon (sprite on top, name across the bottom), holds it for a
configurable number of seconds (default 60), then switches. A rotary encoder
"scrolls" to change the seconds-per-slide.

## Hardware
Full wiring/power reference: `docs/hardware-reference.md`. Pin map lives in
`include/config.h`. HUB75 uses GPIO1–13 + 43; the rotary encoder (electrokit
art. 41021049) uses GPIO44 (A) + GPIO14 (B), with the push button on the onboard
BOOT button (GPIO0) by default.

## Wi-Fi setup (captive portal — no secrets file)
- Credentials are provisioned on-device via **WiFiManager** and stored in NVS by
  the ESP core. There is no `secrets.h` and nothing to compile in.
- First boot (or unknown network): the device opens an AP named `Pokedex-Setup`
  (see `AP_SETUP_SSID`). Join it from a phone, the captive portal pops up, enter
  your Wi-Fi. The panel shows a **setup screen** (State 0) with the AP name +
  portal address while the portal is open.
- The portal has **no timeout** (`setConfigPortalTimeout(0)`) — it stays open
  until you configure it; `autoConnect()` blocks until connected. On a hard
  failure the device reboots and retries. Mid-run drops reconnect silently
  (`netEnsure`) without reopening the portal.
- **Re-configure:** at power-on, press **BOOT** during the ~3 s "press BOOT for
  wifi setup" window (`SETUP_WINDOW_MS`). We poll BOOT *after* boot, not at reset,
  because GPIO0 held at reset enters the ROM download bootloader.

## Build / flash
```
pio run -e s3mini                      # compile
pio run -e s3mini -t upload -t monitor # flash + serial (USB power only)
```
Power rule: never have two 5 V sources at once (laptop USB **or** the 5 V supply,
never both). Keep brightness ~20 while testing on USB; ~90 once on the supply.

## Layout
- `src/main.cpp` — SLIDESHOW ↔ ADJUSTING state machine, NVS-persisted duration.
- `src/net.*` — Wi-Fi + HTTPS sprite fetch into a PSRAM buffer.
- `src/display.*` — HUB75 init, PNGdec decode (composited over black),
  nearest-neighbor scale, name/duration/status text.
- `src/pokemon.*` — random id, sprite URL, fetch/decode orchestration.
- `src/input.*` — ESP32Encoder quadrature + button.
- `src/names.h` — AUTO-GENERATED dex names; regenerate with `python3 tools/gen_names.py`.

## Tuning knobs (config.h)
- `DEX_MIN`/`DEX_MAX` — which Pokémon appear (default full national dex 1–1025).
- `DEFAULT_DURATION_SEC`, `DURATION_MIN_SEC`/`MAX` — slide timing + clamp.
- `ENC_COUNTS_PER_DETENT` — set to match your encoder if one click ≠ one step.
- `PANEL_BRIGHTNESS`, sprite layout constants.

If colors look swapped or the image is shifted, flip `PNG_RGB565_LITTLE_ENDIAN`
in `display.cpp` and/or set `cfg.clkphase` in `displayInit()`.
