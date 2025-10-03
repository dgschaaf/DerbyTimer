#ifndef DISPLAY_H
#define DISPLAY_H

void setupDisplay();
void updateDisplay(uint32_t timeUs, bool isReaction, bool isLeft);
static uint16_t buildPattern(uint8_t digitIndex, uint8_t digitValue, bool showDecimal, bool isReaction, bool isLeft);
static void shiftOut16(uint16_t value);


#endif