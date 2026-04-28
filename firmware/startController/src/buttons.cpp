#include "buttons.h"
#include <Arduino.h>

// Button pin definitions
static const byte buttonLeft 	= 18;  				// Digital pin
static const byte buttonRight 	= 19;  				// Digital pin
static const byte buttonStart 	= A6;  				// Analog pin, Arduino sees A6 as 20
static const byte buttonMode 	= A7;				// Analog pin, Arduino sees A7 as 21

static unsigned long analogCacheTime	= 10;
static unsigned long analogThreshold	= 512;

void setupButtons() {
    pinMode(buttonLeft, INPUT);				// External pull-up
    pinMode(buttonRight, INPUT);			// External pull-up
	// A6, A7 are analog-only, no pinMode needed
}

bool isLeftPressed() {
	// Ideally make this using interrupts, however that requires PCB change to pin 2
	return digitalRead(buttonLeft) == HIGH;	
}
bool isRightPressed() {
	// Ideally make this using interrupts, however that requires PCB change to pin 3
	return digitalRead(buttonRight) == HIGH;
}

bool isStartPressed() {
	// Start uses analog input, poll infrequently to save reading time
    static unsigned long startLastRead = 0;
    static bool startState = false;
    
    if (millis() - startLastRead >= analogCacheTime) {  // 100Hz sampling
        startState = analogRead(buttonStart) > analogThreshold;
        startLastRead = millis();
    }
    return startState;
}

bool isModePressed() {
	// Mode uses analog input, poll infrequently to save reading time
    static unsigned long modeLastRead = 0;
    static bool modeState = false;
    
    if (millis() - modeLastRead >= analogCacheTime) {  // 100Hz sampling
        modeState = analogRead(buttonMode) > analogThreshold;
        modeLastRead = millis();
    }
    return modeState;
}