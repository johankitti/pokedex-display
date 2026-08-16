#pragma once
#include <Arduino.h>

// Look up a display name for a national-dex id (1-based). Returns a pointer to a
// static buffer valid until the next call.
const char* pokemonName(int id);

// Pick a random Pokémon and fetch ONLY its sprite PNG bytes into a freshly
// allocated buffer (no decode/draw), retrying different Pokémon on failure. This
// is the slow, network-bound step, run ahead of time so the display swap is cheap.
// On success fills outName/outId/outBuf/outLen and returns true; the caller owns
// the buffer and must netFree() it. Returns false if every attempt failed.
bool pokemonPrefetch(char* outName, size_t nameLen, int* outId,
                     uint8_t** outBuf, size_t* outLen);
