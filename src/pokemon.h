#pragma once
#include <Arduino.h>

// Look up a display name for a national-dex id (1-based). Returns a pointer to a
// static buffer valid until the next call.
const char* pokemonName(int id);

// Pick random Pokémon, fetch + decode + draw their sprite, retrying on failure.
// On success returns the shown id and copies its name into outName; returns -1
// if every attempt failed (e.g. no network).
int pokemonNext(char* outName, size_t nameLen);
