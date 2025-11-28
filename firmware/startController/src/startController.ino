/*
 * Pinewood Derby Track Start Controller
 * Version: 1.0
 * Author: Darren Schaaf
 * Date September 2025
 * Compile: Arduino IDE 1.8+ or PlatformIO
 * Board: Arduino Nano
 * Libraries Required:
 */

#include <Arduino.h>
#include "lights.h"
#include "gates.h"
#include "serialComm.h"
#include "buttons.h"
#include "globals.h"

// State machine structure for managing state transitions
struct StateMachine {
	raceState current;
	raceState target;
	bool entry;
	bool exit;
	// Returns true when transition is complete
	bool transition(raceState newState) {
		if (!isValidStateTransition(newState)){
			return false;					// Invalid transition
		}
		if (target != newState) {
			target = newState;				// Set intention
		}
		if (current != target) {
			// Try to coordinate transition
			txStatus result = txRaceState(target);
			switch (result) {
				case TX_ACKED:
					entry = true;			// Enter new state
					current = target;		// Commit transition
					exit = true;			// Exit previous state
					resetTxState(MSG_RACE_STATE);
					return true;
				case TX_TIMEOUT:
				case TX_FAILED:
					target = current;		// Abandon transition
					resetTxState(MSG_RACE_STATE);
					// Log error or flash lights
					return false;
				default:
					return false;			// Still pending
			}
		}
		return false;						// Already in target state
	}
	// Handle unsolicited state changes from rxSerial
	void rxTransition(raceState rxState) {
		if (rxState != current) {
			transition(rxState);
		}
	}
	// Add state validation
	bool isValidStateTransition(raceState to) {
		switch(current) {
        	case RACE_IDLE:			return (to == RACE_STAGING 		|| to == RACE_TEST);
        	case RACE_STAGING:		return (to == RACE_COUNTDOWN	|| to == RACE_IDLE);
        	case RACE_COUNTDOWN:	return (to == RACE_RACING);
        	case RACE_RACING:		return (to == RACE_COMPLETE);
        	case RACE_COMPLETE:		return (to == RACE_IDLE);
			case RACE_TEST:			return (to == RACE_IDLE);
        	default:				return false;
    	}
	}
};

// Mode machine structure for managing mode transitions
struct ModeMachine {
	raceMode current;
	raceMode target;
	bool transition(raceMode newMode) {
		// Returns true when transition is complete
		if (target != newMode) {
			target = newMode;					// Set intention
		}
		if (current != target) {
			// Try to coordinate transition
			txStatus result = txRaceMode(target);
			switch (result) {
				case TX_ACKED:
					current = target;			// Commit transition
					resetTxState(MSG_RACE_MODE);
					return true;
				case TX_TIMEOUT:
				case TX_FAILED:
					target = current;			// Abandon transition
					resetTxState(MSG_RACE_MODE);
					// Log error or flash lights
					return false;
				default:
					return false;				// Still pending
			}
		}
		return false;							// Already in target mode
	}
	void nextMode() {
		// Determine next mode in sequence for button press
		switch(current) {
			case MODE_GATEDROP:	target	= MODE_REACTION;	break;
			case MODE_REACTION:	target	= MODE_PRO;			break;
			case MODE_PRO:		target	= MODE_GATEDROP; 	break; // DIALIN not yet defined, will be next = MODE_DIALIIN;
			case MODE_DIALIIN:	target	= MODE_GATEDROP;	break;
			default: 			target	= MODE_GATEDROP;	break;
		}
	}
	void rxTransition(raceMode serialTgt) {
		if (serialTgt != current) {									// Unsolicited mode change from serial
			transition(serialTgt);
		}
	}
};



// globals.h definitions
unsigned long leftReactionTime			= 0;			// Reaction time for left track
unsigned long rightReactionTime			= 0;			// Reaction time for right track
unsigned long raceStartTime				= 0;			// Log the start time of the race
unsigned long leftStartTime				= 0;			// Log the start time of the left track
unsigned long rightStartTime			= 0;			// Log the start time of the right track
bool leftFoul							= false;		// Log foul status of left track
bool rightFoul							= false;		// Log foul status of right track

static_assert(UID_LEN == serialUIDLength, "UID Lengths Must Match");

// State & mode machine instances
static StateMachine stm					= {RACE_IDLE, RACE_IDLE, true, false};
static ModeMachine mdm					= {MODE_GATEDROP, MODE_GATEDROP};

// countdown 
static countdownState cdState			= CD_IDLE;		// current countdownState value - see globals.h
static countdownState prevCdState		= CD_IDLE;		// previous countdownState
static unsigned long cdTimer			= 0;			// countdown timer
static unsigned long stageDelay			= 500;			// default delay between staging sequences
static bool pendStartTx					= false;		// marker for if start transmission is pending

// racing
static bool reactLeftPending			= false;		// marker for if left reaction tx is pending
static bool reactRightPending			= false;		// marker for if right reaction tx is pending
static bool foulStatusPending			= false;		// marker for if foul status tx is pending
static unsigned long startDelay			= 0;			// Unused, delay between start and ACK

// results
static bool winLightsPend				= false;		// marker if result lights need to display
static bool dispAdv						= false;		// marker for pending tx display advance

// button management
static bool startReleased				= true;
static bool modeReleased				= true;


// Helper function to manage countdown timing based on race mode
countdownState tickCountdownState(raceMode mode, countdownState cdState){
	unsigned long currentTime = millis();
	switch (cdState) {
		case CD_STAGED:
			if (mode == MODE_PRO){
				stageDelay = 400;
				cdState = CD_Y1;
			} else {
				stageDelay = 500;
				cdState = CD_Y3;
			}
			cdTimer = currentTime;
			break;
		case CD_Y3:
			if (currentTime - cdTimer >= stageDelay){
				cdState = CD_Y2;
				cdTimer = currentTime;
			}
			break;
		case CD_Y2:
			if (currentTime - cdTimer >= stageDelay){
				cdState = CD_Y1;
				cdTimer = currentTime;
			}
			break;
		case CD_Y1:
			if (currentTime - cdTimer >= stageDelay){
				cdState = CD_GO;
				cdTimer = currentTime;
			}
			break;
		default:
			break;
	}
	return cdState;	
}

// Helper function to calculate elapsed microseconds with overflow protection
unsigned long elapsedMicros(unsigned long startTime, unsigned long endTime) {
    if (endTime >= startTime) {
        return endTime - startTime;  					// Normal case
    } else {
        return (0xFFFFFFFF - startTime) + endTime + 1;	// Overflow case
    }
}

void setup(){
	setupSerial();
	setupButtons();
	setupGates();
	setupLights();
}


void loop(){
	rxSerial();
	switch(stm.current) {
		case RACE_IDLE:
			if(stm.entry){
				stm.entry		= false;
				cdState = CD_IDLE;
				updateLights(LIGHT_OFF);
				dropGate(gateL);										// make sure gate L isn't up
				dropGate(gateR);										// make sure gate R isn't up
				modeReleased	= true;									// assume button not pressed
				startReleased	= true;									// assume button not pressed
			}
			
			// Handle mode changes via button press or rxSerial
			if (!blinkState.active){
				if (isModePressed() && modeReleased){
					modeReleased			= false;						// don't revisit until released
					mdm.nextMode();											// Select mode to advance to per transition order
				}
				if (!isModePressed())		modeReleased	= true; 		// button released, ready for next detection
				mdm.rxTransition(rxMode);									// Handle unsolicited mode changes from rxSerial
				if (mdm.transition(mdm.target)){
					uint8_t pattern;
					switch (mdm.target){
						case MODE_GATEDROP: pattern = LIGHT_Y1; break;
						case MODE_REACTION: pattern = LIGHT_Y2; break;
						case MODE_PRO:      pattern = LIGHT_Y3; break;
						case MODE_DIALIIN:  /* fall-through */
						default:            pattern = LIGHT_GO; break;
					}
					startBlink(pattern, 0x00, 3, 250, LIGHT_OFF);			// blink new mode pattern 3x
					rxMode				= mdm.current;						// update Serial target to match
				}
			}
			updateBlink();

			// Handle state changes via button press or rxSerial
			if (!blinkState.active){
				if (isStartPressed())	stm.target =RACE_STAGING;			// Start moves to STAGING
				stm.rxTransition(rxState);									// Handle unsolicited state changes from rxSerial
				if (stm.transition(stm.target)){
					rxState = stm.current;									// update Serial target to match
				}
			}
			if(stm.exit){
				stm.exit = false;
			}
			break;
			
		case RACE_STAGING:
			if(stm.entry){
				stm.entry			= false;
				returnGates(); 										// reset the gate status to park the cars
				updateLights(LIGHT_BL | LIGHT_BR); 					// set the lights to blue
				blinkState.active 	= false;  						// Clear any pending blinks
			}
			if(gateStatus.returnActive)	returnGates();				// call this until it returnActive is false
			updateBlink();

			// Handle state changes via button press
			if (!blinkState.active){
				if (isStartPressed())	stm.target =RACE_COUNTDOWN;			// Start moves to COUNTDOWN
				if (isModePressed())	stm.target = RACE_IDLE;				// Mode returns to IDLE
			}
			if (stm.transition(stm.target)){
				rxState = stm.current;									// update Serial target to match
			}
			if(stm.exit){
				stm.exit = false;
			}
			break;
			
		case RACE_COUNTDOWN:
			if(stm.entry){
				stm.entry				= false;
				cdState 				= CD_STAGED;
				prevCdState 			= cdState;
			}
			
			// Tick the countdown state.  This function will handle managing stage delays
			// as well as managing the countdown state
			cdState = tickCountdownState(mdm.current, cdState);

			// Watch for the triggers (given right mode).  Drop the gate but store a foul.
			if (mdm.current != MODE_GATEDROP){
				if (isLeftPressed() && gateStatus.leftUp){
					leftStartTime 		= micros();
					leftFoul 			= true;
					dropGate(gateL);
				}
				if (isRightPressed() && gateStatus.rightUp){
					rightStartTime 		= micros();
					rightFoul 			= true;
					dropGate(gateR);
				}
			}

			if (cdState != prevCdState){
				if (cdState == CD_GO){
					stm.transition(RACE_RACING);						// when GO has been hit in countdown, trigger a state transition
					raceStartTime		= micros();						// when GO has been hit in countdown, tell finishController race is started
					pendStartTx			= true;
					resetTxState(MSG_RACE_START);

					if (mdm.current == MODE_GATEDROP){
						// The gate drop mode everyone starts at the same time
						leftStartTime 	= raceStartTime;
						rightStartTime 	= raceStartTime;
						dropGate(gateL);
						dropGate(gateR);
					}
				}

				byte cdLights	= buildLightConfig(cdState, leftFoul, rightFoul, mdm.current);	// set new light pattern
				updateLights(cdLights);									// update lights only when new cdState
				prevCdState = cdState;
			}
			
			if (pendStartTx){
				txStatus strt = txRaceStart(0b0001); 					// helper function handles transmission status
				switch (strt) {
					case TX_ACKED:
						startDelay	= micros();							// Log time delay between actual and ACK start
						pendStartTx 				= false;
						resetTxState(MSG_RACE_START);
						break;
					case TX_TIMEOUT:
					case TX_FAILED:
						pendStartTx 				= false;
						resetTxState(MSG_RACE_START);
						// future: flash red lights for error; updateLights(LIGHT_FL | LIGHT_FR);
						// future: log / transmit state transition error
						break;
					case TX_NONE:
					case TX_SENT:
					case TX_NACKED:
					default:
						break;
				}
			}

			if(stm.exit){
				stm.exit = false;
			}
			break;
			
		case RACE_RACING:
			if(stm.entry){
				stm.entry						= false;
				leftReactionTime				= 0;							// reset reaction time
				rightReactionTime				= 0;							// reset reaction time
				unsigned long raceTimeOffset	= startDelay - raceStartTime;	// not currently used but could compensate for line delays
				foulStatusPending				= true;							// always send foul status
				uint8_t foulMask				= 0;
				if (leftFoul)  foulMask		   |= foul_left;					// add left foul status to mask
				if (rightFoul) foulMask		   |= foul_right;					// add right foul stats to mask
				resetTxState(MSG_RACE_START);
				resetTxState(MSG_FOUL);
				resetTxState(MSG_LEFT_REACT);
				resetTxState(MSG_RIGHT_REACT);
			}
			
			if(rxSerial()){
				// wait for the race complete state transition rx
			}

			if (mdm.current != MODE_GATEDROP){
				// Watch for the triggers (given correct mode).
				if (isLeftPressed() && gateStatus.leftUp){
					leftStartTime 			= micros();
					dropGate(gateL);
					reactLeftPending		= true;
				}
				if (isRightPressed() && gateStatus.rightUp){
					rightStartTime 			= micros();
					dropGate(gateR);
					reactRightPending 		= true;					
				}
				// calculate reaction times, gate drop stays at zero
				if (!gateStatus.leftUp && leftReactionTime == 0){
					if (leftFoul){
						leftReactionTime	= elapsedMicros(raceStartTime, leftStartTime);	// race time is bigger since they started early
					} else {
						leftReactionTime	= elapsedMicros(leftStartTime, raceStartTime);	// normally left start time is bigger
					}
				}
				if (!gateStatus.rightUp && rightReactionTime == 0){
					if (rightFoul){
						rightReactionTime	= elapsedMicros(raceStartTime, rightStartTime);	// race time is bigger since they started early
					} else {
						rightReactionTime	= elapsedMicros(rightStartTime, raceStartTime);	// normally right start time is bigger
					}
				}
			}

			if (!gateStatus.leftUp && !gateStatus.rightUp){
				// only transmit once both tracks have started to avoid detection delays
				if (reactLeftPending){
					txStatus lr = txReactionTime(leftReactionTime,true); 	// helper function handles transmission status
					switch (lr) {
						case TX_ACKED:										
							reactLeftPending		= false;				// transmission complete
							resetTxState(MSG_LEFT_REACT);
							break;
						case TX_TIMEOUT:
						case TX_FAILED:
							reactLeftPending 		= false;				// transmission failed
							resetTxState(MSG_LEFT_REACT);					// reset transmit message
							// future: flash red lights for error; updateLights(LIGHT_FL | LIGHT_FR);
							// future: log / transmit state transition error
							break;
						case TX_NONE:
						case TX_SENT:
						case TX_NACKED:
						default:
							break;
					}
				}
			
				if (reactRightPending){
					txStatus rr = txReactionTime(rightReactionTime,false);
					switch (rr) {
						case TX_ACKED:										
							reactRightPending		= false;				// transmission complete
							resetTxState(MSG_RIGHT_REACT);
							break;
						case TX_TIMEOUT:
						case TX_FAILED:
							reactRightPending 		= false;				// transmission failed
							resetTxState(MSG_RIGHT_REACT);					// reset transmit message
							// future: flash red lights for error; updateLights(LIGHT_FL | LIGHT_FR);
							// future: log / transmit state transition error
							break;
						case TX_NONE:
						case TX_SENT:
						case TX_NACKED:
						default:
							break;
					}
				}
				if (foulStatusPending){
					txStatus f = txFoulStatus(foulMask);
					switch (f) {
						case TX_ACKED:										
							foulStatusPending		= false;				// transmission complete
							resetTxState(MSG_FOUL);
							break;
						case TX_TIMEOUT:
						case TX_FAILED:
							foulStatusPending 		= false;				// transmission failed
							resetTxState(MSG_FOUL);							// reset transmit message
							// future: flash red lights for error; updateLights(LIGHT_FL | LIGHT_FR);
							// future: log / transmit state transition error
							break;
						case TX_NONE:
						case TX_SENT:
						case TX_NACKED:
						default:
							break;
					}
				}
			}

			if (!foulStatusPending && !reactLeftPending && !reactRightPending){
				// wait until all pending messages have been sent until completing transition
				stm.rxTransition(rxState);
			}

			if(stm.exit){
				stm.exit = false;
			}
			break;
			
		case RACE_COMPLETE:
			if(stm.entry){
				stm.entry				= false;
				rxLeftWin				= false;
				rxRightWin				= false;
				rxTie					= false;
				winLightsPend			= false;
				blinkState.active 		= false;  						// Clear any pending blinks
			}
			
			if(rxSerial()){
				// message-specific actions
				if(rxID == MSG_WINNER){
					winLightsPend			= true;						// set the winner lights to display
				}
			}
			
			if (winLightsPend){ 										// Blink lights to show winner
				if(rxLeftWin)	startBlink(LIGHT_GO | LIGHT_FR, LIGHT_FR, 3, 250, LIGHT_GO | LIGHT_FR);
				if(rxRightWin)	startBlink(LIGHT_GO | LIGHT_FL, LIGHT_FL, 3, 250, LIGHT_GO | LIGHT_FL);
				if(rxTie) 		startBlink(LIGHT_GO, 0x00, 3, 250, LIGHT_GO);
				winLightsPend 			= false;
			}
			
			if (isStartPressed() && startReleased){
				startReleased			= false;						// don't revist until button released
				dispAdv					= true;
				resetTxState(MSG_DISP_ADVANCE);
			}
			if (!isStartPressed())		startReleased	= true;			// button released, ready for next detection
			
			if (dispAdv){
					txStatus d = txDisplayAdvance();
					switch (d) {
						case TX_ACKED:										
							dispAdv		= false;						// transmission complete
							resetTxState(MSG_DISP_ADVANCE);
							break;
						case TX_TIMEOUT:
						case TX_FAILED:
							dispAdv 	= false;						// transmission failed
							resetTxState(MSG_DISP_ADVANCE);				// reset transmit message
							break;
						case TX_NONE:
						case TX_SENT:
						case TX_NACKED:
						default:
							break;
					}
				}
				
			if (!winLightsPend && !updateBlink()){				// note: this also executes the updateBlink() function to process blinks
				// wait until all pending messages have been sent until completing transition
				stm.rxTransition(rxState);
			}

			if(stm.exit){
				stm.exit = false;
			}
			break;
			
		case RACE_TEST:  // not currently implemented, return to idle
			if(stm.entry){
				stm.transition(RACE_IDLE);
				stm.entry = false;
			}
			if(stm.exit){
				stm.exit = false;
			}
			break;

		default:
			stm.current = RACE_IDLE;
			break;
	}
}
