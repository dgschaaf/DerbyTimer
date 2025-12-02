/*
 * Pinewood Derby Track Start Controller
 * Version: 1.0
 * Author: Darren Schaaf
 * Date December 2025
 * Compile: Arduino IDE 1.8+ or PlatformIO
 * Board: Arduino Nano AVR
 * Libraries Required:
 */

#include <Arduino.h>
#include "lights.h"
#include "gates.h"
#include "serialComm.h"
#include "buttons.h"
#include "globals.h"

struct StateMachine {
	raceState current;
	raceState target;
	bool entry;
	bool exit;
	bool allowedTransition(raceState next) {
		// Allowed transitions table (FROM x TO)
		static constexpr bool allowed[6][6] = {
		/* FROM\TO:  IDLE STAG CNTD RACE CMPL TEST */
 		/*IDLE*/     {0,   1,   0,   0,   0,   1},
 		/*STAGING*/  {0,   0,   1,   0,   0,   0},
 		/*COUNTDOWN*/{0,   0,   0,   1,   0,   0},
 		/*RACING*/   {0,   0,   0,   0,   1,   0},
 		/*COMPLETE*/ {1,   0,   0,   0,   0,   0},
 		/*TEST*/     {1,   0,   0,   0,   0,   0}
		};

		return allowed[current][next];
	};

	void selfTransition(raceState newState) {
		// 1. Reject illegal transitions
        if (!allowedTransition(newState)) {
			return;
		}

		// 2. Check if already in target state
        if (current == newState) {
            return;
        }

		// 3. Set intention to transition
		target = newState;

		// 4. Attempt coordinated change
		txStatus result	= txRaceState(target);
		switch (result) {
			
			case TX_ACKED:
				// Transition has been confirmed, now commit
				entry 	= true;		// next loop: run entry logic
				current	= target;   // commit new state
				exit 	= true;   		// run exit logic
				resetTxState(MSG_RACE_STATE);
				return;

			case TX_TIMEOUT:
			case TX_FAILED:
				// Transition failed, revert intention and abandon transition
				target	= current;
				resetTxState(MSG_RACE_STATE);
				return;

			default:
				// Still TX_SENT or waiting for ACK
				return;
		}

        return true;  // Already in target state
    }

	void rxTransition(raceState newState) {
		// 1. Check if already in target state
        if (current == newState) {
            return;
        }

		// 2. Commit local state change
		target 			= newState;
		current			= newState;
		entry  			= true;
		exit   			= true;
		return;
	}
};

// Mode machine structure for managing mode transitions
struct ModeMachine {
	raceMode current;
	raceMode target;
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
	uint8_t pattern;

	void selfTransition(raceMode newMode) {
		// 1. Check if already in target state
        if (current == newMode) {
            return;
        }

		// 2. Set intention to transition
		target = newMode;

		// 3. Prepare light pattern
		switch (target){
			case MODE_GATEDROP: pattern = LIGHT_Y1; break;
			case MODE_REACTION: pattern = LIGHT_Y2; break;
			case MODE_PRO:      pattern = LIGHT_Y3; break;
			case MODE_DIALIIN:  /* fall-through */
			default:            pattern = LIGHT_GO; break;
		}

		// 4. Attempt coordinated change
		txStatus result	= txRaceMode(target);
		switch (result) {
			
			case TX_ACKED:
				// Transition has been confirmed, now commit
				current		= target;   						// commit new mode
				rxMode		= current;							// update Serial target to match
				startBlink(pattern, 0x00, 3, 250, LIGHT_OFF);	// blink new mode pattern 3x
				resetTxState(MSG_RACE_MODE);
				return;

			case TX_TIMEOUT:
			case TX_FAILED:
				// Transition failed, revert intention and abandon transition
				target	= current;
				resetTxState(MSG_RACE_MODE);
				return;

			default:
				// Still TX_SENT or waiting for ACK
				return;
		}

        return;  // Already in target mode
	}
	
	void rxTransition(raceMode serialTgt) {
		// 1. Check if already in target mode
        if (serialTgt == current) {
            return;
        }

		// 2. Commit local mode change
		target 			= serialTgt;
		current			= serialTgt;

		// 3. Prepare and execute light pattern
		switch (target){
			case MODE_GATEDROP: pattern = LIGHT_Y1; break;
			case MODE_REACTION: pattern = LIGHT_Y2; break;
			case MODE_PRO:      pattern = LIGHT_Y3; break;
			case MODE_DIALIIN:  /* fall-through */
			default:            pattern = LIGHT_GO; break;
		}
		startBlink(pattern, 0x00, 3, 250, LIGHT_OFF);	// blink new mode pattern 3x

		return;

	}
};

struct raceTimingData {
    uint32_t raceStartUs;
    uint32_t leftStartUs;
    uint32_t rightStartUs;
};

struct raceResultsData {
	uint32_t leftReactUs;
	uint32_t rightReactUs;
	bool leftFoul;
	bool rightFoul;
};

static_assert(UID_LEN == serialUIDLength, "UID Lengths Must Match");

// State & mode machine instances
static StateMachine stm					= {RACE_IDLE, RACE_IDLE, true, false};
static ModeMachine mdm					= {MODE_GATEDROP, MODE_GATEDROP};

// timing
static raceTimingData raceTime			= {0, 0, 0};
static raceResultsData raceResults		= {0, 0, false, false};
uint32_t tNow							= 0;			// current time in microseconds	

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

void startControllerSetup(){
	setupSerial();
	setupButtons();
	setupGates();
	setupLights();

	// Start in idle state.  These variables are declared in globals.h.
	stm.current					= RACE_IDLE;
	stm.target					= RACE_IDLE;
	mdm.current 				= MODE_GATEDROP;
	mdm.target 					= MODE_GATEDROP;
}

// Internal helpers (file-local)
static unsigned long elapsedMicros(unsigned long startTime, unsigned long endTime);
countdownState tickCountdownState(raceMode mode, countdownState cdState);


void startControllerLoop(){
	rxSerial();
	switch(stm.current) {
		case RACE_IDLE:
			if(stm.entry){
				stm.entry		= false;
				cdState 		= CD_IDLE;
				updateLights(LIGHT_OFF);
				dropGate(gateL);										// make sure gate L isn't up
				dropGate(gateR);										// make sure gate R isn't up
				modeReleased	= true;									// assume button not pressed
				startReleased	= true;									// assume button not pressed
			}
			
			updateBlink();
			// Handle mode changes via button press or rxSerial
			if (!blinkState.active){
				if (!isModePressed())		modeReleased	= true;		// button released, ready for next detection
				if (isModePressed() && modeReleased){
					modeReleased			= false;					// don't revisit until released
					mdm.nextMode();										// Select mode to advance to per transition order
				}
				mdm.rxTransition(rxMode);								// Handle unsolicited mode changes from rxSerial
				mdm.selfTransition(mdm.target);							// Handle mode self-transition
			}

			// Handle state changes via button press or rxSerial
			if (!blinkState.active){
				if (isStartPressed())	stm.target = RACE_STAGING;		// Start moves to STAGING
			}

			stm.rxTransition(rxState);									// Handle unsolicited state changes from rxSerial
			stm.selfTransition(stm.target);								// Handle state self-transition

			if(stm.exit){
				stm.exit = false;
			}
			break;
			
		case RACE_STAGING:
			if(stm.entry){
				stm.entry			= false;
				returnGates(); 											// reset the gate status to park the cars
				updateLights(LIGHT_BL | LIGHT_BR); 						// set the lights to blue
				blinkState.active 	= false;  							// Clear any pending blinks
			}

			if(gateStatus.returnActive)	returnGates();					// call this until it returnActive is false

			updateBlink();
			// Handle state changes via button press
			if (!blinkState.active){
				if (isStartPressed())	stm.target = RACE_COUNTDOWN;	// Start moves to COUNTDOWN
				if (isModePressed())	stm.target = RACE_IDLE;			// Mode returns to IDLE
			}

			stm.selfTransition(stm.target);								// transitions state if updated target

			if(stm.exit){
				stm.exit = false;
			}
			break;
			
		case RACE_COUNTDOWN:
			if(stm.entry){
				stm.entry				= false;
				cdState 				= CD_STAGED;
				prevCdState 			= cdState;
				startDelay				= 0;
			}
			
			tNow = micros();

			// Tick the countdown state.  This function will handle managing stage delays
			// as well as managing the countdown state
			cdState = tickCountdownState(mdm.current, cdState);

			// Watch for the triggers (given right mode).  Drop the gate but store a foul.
			if (mdm.current != MODE_GATEDROP){
				if (isLeftPressed() && gateStatus.leftUp){
					raceTime.leftStartUs	= tNow;
					raceResults.leftFoul	= true;
					dropGate(gateL);
				}
				if (isRightPressed() && gateStatus.rightUp){
					raceTime.rightStartUs	= tNow;
					raceResults.rightFoul	= true;
					dropGate(gateR);
				}
			}

			if (cdState != prevCdState){
				if (cdState == CD_GO){
					stm.target = RACE_RACING					// when GO has been hit in countdown, trigger a state transition
					raceTime.raceStartUs	= tNow;				// when GO has been hit in countdown, tell finishController race is started
					pendStartTx				= true;
					resetTxState(MSG_RACE_START);

					if (mdm.current == MODE_GATEDROP){
						// The gate drop mode everyone starts at the same time
						raceTime.leftStartUs	= tNow;
						raceTime.rightStartUs	= tNow;
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
						startDelay		= micros();						// Log time delay between actual and ACK start
						pendStartTx 	= false;
						resetTxState(MSG_RACE_START);
						break;
					case TX_TIMEOUT:
					case TX_FAILED:
						pendStartTx 	= false;
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

			stm.selfTransition(stm.target);								// transitions state if updated target

			if(stm.exit){
				stm.exit = false;
			}
			break;
			
		case RACE_RACING:
			if(stm.entry){
				stm.entry						= false;
				raceResults.rightReactUs		= 0;									// reset reaction time
				raceResults.leftReactUs		= 0;									// reset reaction time
				//unsigned long raceTimeOffset	= startDelay - raceTime.raceStartUs;	// not currently used but could compensate for line delays
				foulStatusPending				= true;									// always send foul status
				uint8_t foulMask				= 0;
				if (leftFoul)  foulMask		   |= foul_left;							// add left foul status to mask
				if (rightFoul) foulMask		   |= foul_right;							// add right foul stats to mask
				resetTxState(MSG_RACE_START);
				resetTxState(MSG_FOUL);
				resetTxState(MSG_LEFT_REACT);
				resetTxState(MSG_RIGHT_REACT);
			}

			tNow 							= micros();

			if (mdm.current != MODE_GATEDROP){
				// Watch for the triggers (given correct mode).
				if (isLeftPressed() && gateStatus.leftUp){
					leftStartTime 			= tNow;
					dropGate(gateL);
					reactLeftPending		= true;
				}
				if (isRightPressed() && gateStatus.rightUp){
					rightStartTime 			= tNow;
					dropGate(gateR);
					reactRightPending 		= true;					
				}
				// calculate reaction times, gate drop stays at zero
				if (!gateStatus.leftUp && raceResults.leftReactUs == 0){
					if (leftFoul){
						raceResults.leftReactUs		= elapsedMicros(raceTime.raceStartUs, raceTime.leftStartUs);	// race time is bigger since they started early
					} else {
						raceResults.leftReactUs		= elapsedMicros(raceTime.leftStartUs, raceTime.raceStartUs);	// normally left start time is bigger
					}
				}
				if (!gateStatus.rightUp && raceResults.rightReactUs == 0){
					if (rightFoul){
						raceResults.rightReactUs	= elapsedMicros(raceTime.raceStartUs, raceTime.rightStartUs);	// race time is bigger since they started early
					} else {
						raceResults.rightReactUs	= elapsedMicros(raceTime.rightStartUs, raceTime.raceStartUs);	// normally right start time is bigger
					}
				}
			}

			if (!gateStatus.leftUp && !gateStatus.rightUp){
				// only transmit once both tracks have started to avoid detection delays
				if (reactLeftPending){
					txStatus lr = txReactionTime(raceResults.leftReactUs,true); 	// helper function handles transmission status
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
					txStatus rr = txReactionTime(raceResults.rightReactUs,false);
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
				stm.selfTransition(RACE_IDLE);
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

/* =========================================================================
 *                        RACE_IDLE HELPER FUNCTIONS
 * ========================================================================= */
 
 /* =========================================================================
 *                        RACE_STAGING HELPER FUNCTIONS
 * ========================================================================= */

/* =========================================================================
 *                        RACE_COUNTDOWN HELPER FUNCTIONS
 * ========================================================================= */

countdownState tickCountdownState(raceMode mode, countdownState cdState){
	// Helper function to manage countdown timing based on race mode
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

/* =========================================================================
 *                        RACE_RACING HELPER FUNCTIONS
 * ========================================================================= */

unsigned long elapsedMicros(unsigned long startTime, unsigned long endTime) {
	// Helper function to calculate elapsed microseconds with overflow protection
    if (endTime >= startTime) {
        return endTime - startTime;  					// Normal case
    } else {
        return (0xFFFFFFFF - startTime) + endTime + 1;	// Overflow case
    }
}

 /* =========================================================================
 *                        RACE_COMPLETE HELPER FUNCTIONS
 * ========================================================================= */

 /* =========================================================================
 *                        GENERIC HELPER FUNCTIONS
 * ========================================================================= */