#pragma once
#include <Arduino.h>

// Bring up Wi-Fi. Credentials are provisioned via the on-device captive portal
// (WiFiManager) and stored in NVS by the ESP core — no secrets.h needed.
//   forcePortal = true  -> always open the "Pokedex-Setup" AP (BOOT-held setup)
//   forcePortal = false -> try saved creds, only open the portal if none/failed
// Returns true once connected as a station.
bool netStart(bool forcePortal);

// Reconnect using saved creds if the link dropped (never opens the portal).
bool netEnsure();

// HTTP(S) GET `url` into a freshly-allocated buffer (PSRAM preferred).
// On success sets *outBuf/*outLen and returns true; caller must netFree(*outBuf).
bool httpGetBinary(const char* url, uint8_t** outBuf, size_t* outLen);

// Free a buffer returned by httpGetBinary().
void netFree(uint8_t* buf);
