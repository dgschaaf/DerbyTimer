#include "buttons.h"
#include <Arduino.h>

// Button pin definitions. All four inputs idle HIGH (pull-up on the shield)
// and a press connects the input to GND, so pressed reads LOW.
static const byte buttonLeft 	= 18;  				// Digital pin
static const byte buttonRight 	= 19;  				// Digital pin
static const byte buttonStart 	= A7;  				// Analog-only pin
static const byte buttonMode 	= A6;				// Analog-only pin

static constexpr unsigned long analogPollIntervalMs = 10;   // re-read the analog buttons at ~100 Hz
static constexpr int           analogThreshold      = 512;  // ADC midpoint; a read below this counts as pressed

// Click state for the two operator buttons. Refreshed once per loop pass by
// updateButtons(); the flags describe that pass only.
static ClickDetector startClick;
static ClickDetector modeClick;
static bool startClickNow = false;
static bool modeClickNow  = false;

void setupButtons() {
    pinMode(buttonLeft, INPUT);				// External pull-up
    pinMode(buttonRight, INPUT);			// External pull-up
	// A6, A7 are analog-only, no pinMode needed
}

// Call once at the top of the loop, before any handler reads a click.
void updateButtons() {
	startClickNow = startClick.update(isStartPressed());
	modeClickNow  = modeClick.update(isModePressed());
}

bool startClicked() { return startClickNow; }
bool modeClicked()  { return modeClickNow; }

bool isLeftPressed() {
	// Ideally make this using interrupts, however that requires PCB change to pin 2
	return digitalRead(buttonLeft) == LOW;	
}
bool isRightPressed() {
	// Ideally make this using interrupts, however that requires PCB change to pin 3
	return digitalRead(buttonRight) == LOW;
}

bool isStartPressed() {
	// Start uses analog input, poll infrequently to save reading time
    static unsigned long startLastRead = 0;
    static bool startState = false;
    
    if (millis() - startLastRead >= analogPollIntervalMs) {
        startState = analogRead(buttonStart) < analogThreshold;
        startLastRead = millis();
    }
    return startState;
}

bool isModePressed() {
	// Mode uses analog input, poll infrequently to save reading time
    static unsigned long modeLastRead = 0;
    static bool modeState = false;
    
    if (millis() - modeLastRead >= analogPollIntervalMs) {
        modeState = analogRead(buttonMode) < analogThreshold;
        modeLastRead = millis();
    }
    return modeState;
}