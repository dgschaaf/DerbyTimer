#include "gates.h"

// Gate pins
const byte gateL 		= 4;
const byte gateR 		= 7;
const byte gateReturn	= 6;

gateStatusInfo gateStatus	= {false, false, false, 0, 500};	// initialize with 500 ms wait time

void setupGates() {

    pinMode(gateL, OUTPUT);
    pinMode(gateR, OUTPUT);
    pinMode(gateReturn, OUTPUT);
    digitalWrite(gateL, LOW);
    digitalWrite(gateR, LOW);
    digitalWrite(gateReturn, LOW);
}

void dropGate(byte gatePin) {
    digitalWrite(gatePin, LOW);				// de-energize electromagnet, spring pulls gate down
	if (gatePin == gateL)	gateStatus.leftUp	= false;
	if (gatePin == gateR)	gateStatus.rightUp	= false;
}

void returnGates() {
	unsigned long now = millis();
	if(gateStatus.returnActive){
		if(now - gateStatus.returnTime > gateStatus.waitTime){
			digitalWrite(gateReturn, LOW);		// deactivate solenoid
			gateStatus.returnActive 	= false;
		}
	} else if(!gateStatus.leftUp || !gateStatus.rightUp){
		digitalWrite(gateL, HIGH);				// energize electromagnet to hold gate
		digitalWrite(gateR, HIGH);				// energize electromagnet to hold gate
		digitalWrite(gateReturn,HIGH);			// solenoid pushes both gates into magnets
		gateStatus.leftUp				= true;
		gateStatus.rightUp				= true;
		gateStatus.returnActive			= true;
		gateStatus.returnTime			= now;
	}
}