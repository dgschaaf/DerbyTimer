#include "lights.h"

// Shift register pins
static const byte dataPin 		= 2;
static const byte clockPin 		= 3;
static const byte latchPin 		= 5;
BlinkState blinkState 			= {0, 0, 0, 0, 0, 250, false, false, 0};

void setupLights() {
    pinMode(dataPin, OUTPUT);
    pinMode(clockPin, OUTPUT);
    pinMode(latchPin, OUTPUT);
    lightsOff();
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

void lightTestPattern() {
    Serial.println(F("Running light pattern test..."));

    const unsigned long delayTime = 1000;

    // All individual lights
    byte patterns[] = {
        LIGHT_BL,
        LIGHT_BR,
        LIGHT_Y3,
        LIGHT_Y2,
        LIGHT_Y1,
        LIGHT_GO,
        LIGHT_FL,
        LIGHT_FR,
        LIGHT_GO | LIGHT_FL,   			// Green + Red Left
        LIGHT_GO | LIGHT_FR,   			// Green + Red Right
        LIGHT_FL | LIGHT_FR,   			// Both Red
        LIGHT_Y1 | LIGHT_Y2 | LIGHT_Y3,	// PRO Mode
        LIGHT_BL | LIGHT_BR, 			// Staged mode
        LIGHT_GO | LIGHT_FL | LIGHT_FR,	// GO with both fouls
        LIGHT_OFF                 		// All off
    };

    const int numPatterns = sizeof(patterns) / sizeof(patterns[0]);

    for (int i = 0; i < numPatterns; i++) {
        updateLights(patterns[i]);
        Serial.print(F("Pattern ")); Serial.println(i);
        delay(delayTime);
    }

    Serial.println(F("Light pattern test complete."));
}


void startBlink(byte pattern1, byte pattern2, uint8_t count, uint16_t rate, byte finalPattern) {
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