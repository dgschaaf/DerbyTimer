#include "display.h"
#include <Arduino.h>

// Shift register pins – update these to match your wiring.  
// The data pin feeds serial data into the first 74HC595, 
// the clock pin triggers the shift of data bits, 
// and the latch pin transfers the shifted data to the parallel outputs when pulsed high then low.
static const uint8_t SHIFT_DATA_PIN  = 10;
static const uint8_t SHIFT_CLK_PIN   = 9;
static const uint8_t SHIFT_LATCH_PIN = 8;

void setupDisplay() {
	// Configure shift register control pins.  
	// Use OUTPUT mode so they can drive the registers.  
	// The latch pin is low by default.
    pinMode(SHIFT_DATA_PIN,  OUTPUT);
    pinMode(SHIFT_CLK_PIN,   OUTPUT);
    pinMode(SHIFT_LATCH_PIN, OUTPUT);
    digitalWrite(SHIFT_LATCH_PIN, LOW);

    // Blank the displays on startup.  
    updateDisplay(0, false, true);
    updateDisplay(0, false, false);
}

void updateDisplay(uint32_t timeUs, bool isReaction, bool isLeft) {
	// Rounds time in us to ms, converts to BCD, and writes to external display based on lane (left/right) and value (time/reaction)
	
	uint32_t timeMs		= (timeUs + 500) / 1000;			// round time to the nearest millisecond
	if (timeMs > 99999) {									// clamp time at 99.999 to avoid overflow on the display
		timeMs = 99999;
	}
    uint8_t digits[5];										// Extract five BCD digits.  Each digit is in the range [0,9].
    digits[0] = (timeMs / 10000) % 10; // tens of seconds
    digits[1] = (timeMs /  1000) % 10; // ones of seconds
    digits[2] = (timeMs /   100) % 10; // tenths
    digits[3] = (timeMs /    10) % 10; // hundredths
    digits[4] =  timeMs          % 10; // thousandths

	// Iterate over each digit and shift it out.
	// Digit index corresponds to demux address lines A0..A2 on U3.  
	// The decimal flag is asserted when the digit index is 1 (between ones and tenths).
    for (uint8_t idx = 0; idx < 5; ++idx) {
        bool showDec = (idx == 1);
        uint16_t pat = buildPattern(idx, digits[idx], showDec, isReaction, isLeft);
        shiftOut16(pat);
    }
}

//****
//****Current code shifts out each digit separately Consider buffering all digits and updating in one operation****
//****

uint16_t buildPattern(uint8_t digitIndex, uint8_t digitValue, bool showDecimal, bool isReaction, bool isLeft) {
	// Build a 16‑bit pattern to send to the chained 74HC595 shift registers.
	// Bits [15..12] map to U1 QE..QH which carry the 4‑bit BCD value (AD0..AD3).
	// Bits [11..8]  map to U1 QA..QD which enable decimal points on the left and right displays for time and reaction measurements.
	// Bits [7..5]   (U3 QF..QH) are unused and set to 0.
	// Bits [4]      (U3 QE) indicates lane: 0=left, 1=right.
	// Bits [3]      (U3 QD) indicates measurement type: 0=time, 1=reaction.
	// Bits [2..0]   (U3 QC..QA) are the 3‑bit digit index for the 74HC137.
	
	// Helper masks for the decimal flags (upper nibble of U1).
	// These map to U1 QA..QD respectively and control the decimal point on each display.  
	// Adjust the mapping if your PCB routes the flags differently.
	static const uint8_t DECIMAL_LEFT_TIME   = 0b0001; // U1 QA – decimal on left lane time
	static const uint8_t DECIMAL_RIGHT_TIME  = 0b0010; // U1 QB – decimal on right lane time
	static const uint8_t DECIMAL_LEFT_REACT  = 0b0100; // U1 QC – decimal on left lane reaction
	static const uint8_t DECIMAL_RIGHT_REACT = 0b1000; // U1 QD – decimal on right lane reaction
	
    uint16_t pattern = 0;
    uint8_t lower = 0;					// Lower byte – U3 outputs (bits 0..7)
    lower |= (digitIndex & 0x07);		// Bits 0..2: digit address A0..A2
    if (isReaction) {					// Bit 3: time(0)/reaction(1) select
        lower |= (1 << 3);
    }
    if (!isLeft) {						// Bit 4: lane select.  0=left, 1=right.
        lower |= (1 << 4);
    }
    pattern |= lower;					// Bits 5..7 remain zero.
    uint16_t bcd = (digitValue & 0x0F);	// Upper byte – U1 outputs.  Bits 15..12: BCD value on AD lines QE..QH.
    pattern |= (bcd << 12);				// Bits 11..8: decimal flags on QA..QD.
    uint8_t decFlags = 0;
    if (showDecimal) {
        if (!isReaction && isLeft)   decFlags |= DECIMAL_LEFT_TIME;
        if (!isReaction && !isLeft)  decFlags |= DECIMAL_RIGHT_TIME;
        if (isReaction && isLeft)    decFlags |= DECIMAL_LEFT_REACT;
        if (isReaction && !isLeft)   decFlags |= DECIMAL_RIGHT_REACT;
    }
    pattern |= (uint16_t)(decFlags << 8);
    return pattern;
}

void shiftOut16(uint16_t value) {
	// Shift a 16‑bit value into the chained 74HC595s.  Bits are transmitted MSB first.  
	// After shifting, the latch pin is toggled high then low to update the outputs.  
	// Adjust the latch timing if needed for your particular hardware (some designs require a short delay).
	
    digitalWrite(SHIFT_LATCH_PIN, LOW);			// Bring latch low while shifting.
    for (int8_t bit = 15; bit >= 0; --bit) {	// Shift out 16 bits.  Start with the MSB (bit15)
        digitalWrite(SHIFT_CLK_PIN, LOW);		// Write data bit
        bool bitVal = (value >> bit) & 0x01;
        digitalWrite(SHIFT_DATA_PIN, bitVal ? HIGH : LOW);	
        digitalWrite(SHIFT_CLK_PIN, HIGH);		// Clock the bit into the register.
    }
    digitalWrite(SHIFT_LATCH_PIN, HIGH);		// Latch the shifted value to the outputs.
    digitalWrite(SHIFT_LATCH_PIN, LOW);			// Optionally insert a small delay here (e.g., delayMicroseconds(1)).
}