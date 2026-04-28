#ifndef DISPLAY_H
#define DISPLAY_H

/**
 * @brief Configuration for the race results display.
 *
 * Update one full lane: show a 32-bit time value (µs), converted to MM.MMM
 * isLeft = true → left lane, false → right lane
 */

// Public API
void setupDisplay();
void updateDisplay(uint32_t timeUs, bool isLeft);
void clearDisplay(bool isLeft);

#endif // DISPLAY_H