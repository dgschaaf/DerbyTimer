#ifndef LIGHTS_H
#define LIGHTS_H

/**
 * @brief Configuration for the christmas tree light control.
 *
 * Several light patterns are defined to make configuring the shift register easier.
 * Functions exsit to start and service blinking patterns.
 * 
 */

struct BlinkState {
    byte pattern1;
    byte pattern2;
    uint8_t count;
    uint8_t remaining;
    unsigned long lastToggle;
    uint16_t rate;
    bool active;
    bool toggle;
    byte finalPattern;
};

// Shift register bit layout (Q0–Q7)
#define LIGHT_OFF 0x00		// all zeros to the shift register
#define LIGHT_BR   (1 << 0) // Q0: Blue R
#define LIGHT_BL   (1 << 1) // Q1: Blue L
#define LIGHT_Y1   (1 << 2)	// Q2: Y1
#define LIGHT_Y2   (1 << 3) // Q3: Y2
#define LIGHT_Y3   (1 << 4) // Q4: Y3
#define LIGHT_GO   (1 << 5) // Q5: GO (Green)
#define LIGHT_FL   (1 << 6) // Q6: Red L
#define LIGHT_FR   (1 << 7) // Q7: Red R

// Global blink state instance (defined in lights.cpp)
extern BlinkState blinkState;

// Setup/teardown
void setupLights();

// Public API
void updateLights(byte config);
byte buildLightConfig(countdownState state, bool FL, bool FR, raceMode mode);
void lightTestPattern();
void startBlink(byte pattern1, byte pattern2, uint8_t count, uint16_t rate, byte finalPattern);
bool updateBlink();

#endif  // LIGHTS_H