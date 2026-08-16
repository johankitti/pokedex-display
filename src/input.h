#pragma once
#include <Arduino.h>

// Set up the rotary encoder (quadrature) and push button.
void inputInit();

// Net detent steps turned since the last call: +N clockwise, -N counter-clockwise,
// 0 if the knob hasn't moved a full detent.
int inputReadDelta();

// True exactly once per debounced button press (falling edge).
bool inputButtonPressed();
