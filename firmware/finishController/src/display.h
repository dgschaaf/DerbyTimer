#ifndef DISPLAY_H
#define DISPLAY_H

#include "raceTypes.h"

// Public API
void setupDisplay();
void updateDisplay(uint32_t timeUs, Lane lane);
void clearDisplay(Lane lane);

#endif // DISPLAY_H