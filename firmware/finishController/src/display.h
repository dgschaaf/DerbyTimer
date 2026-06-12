#ifndef DISPLAY_H
#define DISPLAY_H

#include "raceTypes.h"

// Number of 7-segment digits per lane: tens, ones, tenths, hundredths, thousandths
constexpr uint8_t DISPLAY_NUM_DIGITS = 5;

// Maximum displayable value in ms. 99999500 us would round to 100000 ms
// (6 digits), so clamp one count below the 5-digit ceiling.
constexpr uint32_t DISPLAY_MAX_MS = 99998;

// Pure digit extraction: time in microseconds -> 5 BCD digit values,
// rounded to the nearest millisecond and clamped to DISPLAY_MAX_MS.
// d[0]=tens, d[1]=ones, d[2]=tenths, d[3]=hundredths, d[4]=thousandths.
// Exposed here so test_display can verify rounding/clamping natively.
inline void extractDisplayDigits(uint32_t timeUs, uint8_t d[DISPLAY_NUM_DIGITS]) {
    // Clamp before rounding: near UINT32_MAX the +500 rounding offset would
    // wrap and display a tiny value instead of the ceiling.
    uint32_t tMs = (timeUs > DISPLAY_MAX_MS * 1000u + 499u)
                 ? DISPLAY_MAX_MS
                 : (timeUs + 500) / 1000;

    d[0] = (tMs / 10000) % 10;  // tens
    d[1] = (tMs /  1000) % 10;  // ones
    d[2] = (tMs /   100) % 10;  // tenths
    d[3] = (tMs /    10) % 10;  // hundredths
    d[4] =  tMs          % 10;  // thousandths
}

// Public API
void setupDisplay();
void updateDisplay(uint32_t timeUs, Lane lane);
void clearDisplay(Lane lane);

#endif // DISPLAY_H