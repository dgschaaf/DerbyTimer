/*
 * DerbyTimer DUT Responder / Protocol Echo
 * =========================================
 * 
 * Purpose: Acts as a simple protocol responder when testing the 
 *          startController or finishController's TX functions.
 *          Can run on either controller or a separate test Arduino.
 * 
 * Mode 1 - Echo Mode (default):
 *   Receives any valid message and sends ACK
 *   Prints received messages to USB Serial for monitoring
 * 
 * Mode 2 - Stimulus Mode:
 *   Sends specific messages to trigger DUT responses
 *   Useful for testing DUT's RX handling
 * 
 * Wiring (when used as standalone responder):
 *   Responder TX (Pin 1 or SoftSerial) --> DUT RX
 *   Responder RX (Pin 0 or SoftSerial) --> DUT TX
 *   GND --> GND
 * 
 * For Arduino Nano 33 BLE:
 *   Use Serial1 for DUT communication
 *   Use Serial (USB) for monitoring
 */

#include <Arduino.h>

// ==================== BOARD CONFIGURATION ====================
// Uncomment ONE of these based on your board

// #define USE_SOFTWARE_SERIAL    // For Uno/Nano AVR with USB monitoring
#define USE_HARDWARE_SERIAL    // For Nano 33 BLE using Serial1

#ifdef USE_SOFTWARE_SERIAL
#include <SoftwareSerial.h>
#define SOFT_RX_PIN  2
#define SOFT_TX_PIN  3
SoftwareSerial dutSerial(SOFT_RX_PIN, SOFT_TX_PIN);
#else
#define dutSerial Serial1       // Nano 33 BLE: TX=Pin1, RX=Pin0
#endif

// ==================== PROTOCOL DEFINITIONS ====================

enum raceState : uint8_t { 
    RACE_IDLE, RACE_STAGING, RACE_COUNTDOWN, RACE_RACING, RACE_COMPLETE, RACE_TEST
};

enum raceMode : uint8_t {
    MODE_GATEDROP, MODE_REACTION, MODE_PRO, MODE_DIALIIN 
};

// ==================== CONFIGURATION ====================
#define BAUD_RATE  115200

// Current state tracking
raceState currentState = RACE_IDLE;
raceMode currentMode = MODE_GATEDROP;
bool raceStarted = false;
uint32_t leftReactionTime = 0;
uint32_t rightReactionTime = 0;
bool leftFoul = false;
bool rightFoul = false;
bool leftWin = false;
bool rightWin = false;
bool tie = false;

// ==================== STIMULUS FUNCTIONS ====================

void sendStateChange(raceState state) {
    uint8_t payload = state;
	txStatus txRaceState(state); 
    //sendMessage(MSG_RACE_STATE, &payload, 1);
}
void sendModeChange(raceMode mode) {
    uint8_t payload = mode;
	txStatus txRaceMode(mode);
    //sendMessage(MSG_RACE_MODE, &payload, 1);
}
void sendRaceStart(uint8_t mask) {
	txStatus txRaceStart(mask);
    //sendMessage(MSG_RACE_START, &mask, 1);
}
void sendReactionTime(bool isLeft, uint32_t timeUs) {
	txStatus txReactionTime(isLeft, reactionTime);
    //sendMessage(isLeft ? MSG_LEFT_REACT : MSG_RIGHT_REACT, payload, 4);
}
void sendFoul(uint8_t mask) {
	txStatus txFoulStatus(mask);
    //sendMessage(MSG_FOUL, &mask, 1);
}
void sendWinner(uint8_t mask) {
	txStatus txWinner(mask);
    //sendMessage(MSG_WINNER, &mask, 1);
}
void sendError(uint8_t mask) {
	txStatus txError();
}
void sendResult(){
	txStatus txResult()
}
// Do not need for txStatus txCarID since it is a growth function
// Do not need for txDisplayAdvance() since there is no payload
// Do not need for txAck() since there is no payload
// Do not need for txNack() since there is no payload


// ==================== SETUP & LOOP ====================

void setup() {
    dutSerial.begin(BAUD_RATE);
    delay(1000);
}

void loop() {
	rxSerial();  // Process incoming messages from DUT
	// Echo back payload for confirmation
	switch (rxID) {
		case MSG_RACE_STATE:
			sendStateChange(state);
			break;
		case MSG_RACE_MODE:
			sendModeChange(mode);
			break;
		case MSG_LEFT_REACT:
		case MSG_RIGHT_REACT:
			sendReactionTime(isLeft, reactionTime);
			break;
		case MSG_LEFT_RESTULT:
		case MSG_RIGHT_RESULT:
			sendResult();
			break;
		case MSG_FOUL:
			sendFoul(mask);
			break;
		case MSG_WINNER:
			sendWinner(mask);
			break;
		case MSG_ERROR:
			sendError(mask);
			break;
		default:
			break;
	
}