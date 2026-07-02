#include "lights.h"
#include <Arduino.h>

// Shift register pins
static const byte dataPin 		= 2;
static const byte clockPin 		= 3;
static const byte latchPin 		= 5;
BlinkState blinkState 			= {0, 0, 0, 0, 0, 250, false, false, 0};

void setupLights() {
    pinMode(dataPin, OUTPUT);
    pinMode(clockPin, OUTPUT);
    pinMode(latchPin, OUTPUT);
    updateLights(LIGHT_OFF); // Start with all lights off
}

void updateLights(byte config){
    digitalWrite(latchPin, LOW);
    shiftOut(dataPin, clockPin, MSBFIRST, config);
    digitalWrite(latchPin, HIGH);
}

byte buildLightConfig(countdownState state, bool FL, bool FR, raceMode mode) {
    byte config = LIGHT_BL | LIGHT_BR;  // Always show blue lights

    switch (state) {
        case CD_Y3: config |= LIGHT_Y3; break;
        case CD_Y2: config |= LIGHT_Y2; break;
        case CD_Y1: 
			if (mode == MODE_PRO){
				config |= (LIGHT_Y3 | LIGHT_Y2 | LIGHT_Y1);
			} else {
				config |= LIGHT_Y1;
			}
			break;
        case CD_GO: config |= LIGHT_GO;  // Green L, Green R
            break;
        default: break;
    }

    // Add red indicators as overlays
    if (FL) config |= LIGHT_FL;
    if (FR) config |= LIGHT_FR;

    return config;
}

void cancelBlink() { blinkState.active = false; }
bool isBlinking()  { return blinkState.active; }

void startBlink(byte pattern1, byte pattern2, uint8_t count, uint16_t rate, byte finalPattern) {
    if (count == 0) {                 // zero blinks: just apply the final pattern
        blinkState.active = false;    // (guards the remaining counter against wrapping)
        updateLights(finalPattern);
        return;
    }
    blinkState.pattern1 = pattern1;
    blinkState.pattern2 = pattern2;
    blinkState.count = count;
    blinkState.remaining = count * 2; // Each blink is 2 states
    blinkState.rate = rate;
    blinkState.active = true;
    blinkState.toggle = false;
    blinkState.finalPattern = finalPattern;
    blinkState.lastToggle = millis();
    updateLights(pattern1);
}

bool updateBlink() {
    if (!blinkState.active) return false;
    
    unsigned long now = millis();
    if (now - blinkState.lastToggle >= blinkState.rate) {
        blinkState.lastToggle = now;
        blinkState.toggle = !blinkState.toggle;
        
        updateLights(blinkState.toggle ? blinkState.pattern2 : blinkState.pattern1);
        
        if (--blinkState.remaining == 0) {
            blinkState.active = false;
            updateLights(blinkState.finalPattern);
            return false; // Blink complete
        }
    }
    return true; // Still blinking
}