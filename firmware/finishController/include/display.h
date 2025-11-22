#ifndef DISPLAY_H
#define DISPLAY_H
#include <Arduino.h>

void setupDisplay();
// Update one full lane: show a 32-bit time value (µs), converted to MM.MMM
// isLeft = true → left lane, false → right lane
void updateDisplay(uint32_t timeUs, bool isLeft);
void clearDisplay(bool isLeft);