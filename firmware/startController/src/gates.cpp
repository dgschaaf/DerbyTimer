#include "gates.h"
#include <Arduino.h>

static const byte pinGateL        	= 4;
static const byte pinGateR        	= 7;
static const byte pinGateReturn   	= 6;
static const uint16_t returnWaitMs	= 500;

struct GateState {
	bool returnActive;
	bool leftUp;
	bool rightUp;
	unsigned long returnTime;
};
static GateState gs = {false, false, false, 0};

void setupGates() {
	pinMode(pinGateL, OUTPUT);
	pinMode(pinGateR, OUTPUT);
	pinMode(pinGateReturn, OUTPUT);
	digitalWrite(pinGateL, LOW);
	digitalWrite(pinGateR, LOW);
	digitalWrite(pinGateReturn, LOW);
}

bool isLaneUp(Lane lane) {
	return (lane == LANE_LEFT) ? gs.leftUp : gs.rightUp;
}

bool areLanesReady() {
	// future: also check RFID car-present sensors per lane
	return !gs.returnActive && gs.leftUp && gs.rightUp;
}

void dropGate(Lane lane) {
	if (!isLaneUp(lane)) return;
	if (lane == LANE_LEFT) {
		digitalWrite(pinGateL, LOW);
		gs.leftUp = false;
	} else {
		digitalWrite(pinGateR, LOW);
		gs.rightUp = false;
	}
}

void returnGates() {
	if (gs.returnActive) return;
	if (gs.leftUp && gs.rightUp) return;
	digitalWrite(pinGateL, HIGH);
	digitalWrite(pinGateR, HIGH);
	digitalWrite(pinGateReturn, HIGH);
	gs.leftUp       = true;
	gs.rightUp      = true;
	gs.returnActive = true;
	gs.returnTime   = millis();
}

void updateGates(unsigned long now) {
	if (!gs.returnActive) return;
	if (now - gs.returnTime >= returnWaitMs) {
		digitalWrite(pinGateReturn, LOW);
		gs.returnActive = false;
	}
}
