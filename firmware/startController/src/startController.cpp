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
#include "raceTypes.h"
#include "serialComm.h"
#include "stateMachine.h"
#include "lights.h"
#include "gates.h"
#include "buttons.h"

// Mode machine structure for managing mode transitions
struct modeMachine {
	raceMode current;
	raceMode target;
	void nextMode() {
		// Determine next mode in sequence for button press
		switch(current) {
			case MODE_GATEDROP:	target	= MODE_REACTION;	break;
			case MODE_REACTION:	target	= MODE_PRO;			break;
			case MODE_PRO:		target	= MODE_GATEDROP;	break; // DIALIIN is skipped — only Race Manager can enter it via BLE
			case MODE_DIALIIN:	target	= MODE_GATEDROP;	break; // exit path: operator can press mode to leave DIALIIN
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
		txStatus result	= txRaceMode((uint8_t)target);
		switch (result) {
			
			case TX_ACKED:
				// Transition has been confirmed, now commit
				current		= target;   						// commit new mode
				rx.Mode		= (uint8_t)current;					// update Serial target to match
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
		if (serialTgt >= MODE_COUNT) {
			txNack(MSG_RACE_MODE);
			return;
		}
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

struct PendingTx {
	bool raceStart  = false;
	bool foulStatus = false;
	bool leftReact  = false;
	bool rightReact = false;
	bool dispAdv    = false;

	bool anyPending() const {
		return raceStart || foulStatus || leftReact || rightReact || dispAdv;
	}
	void queue(serialMsgID id) {
		switch(id) {
			case MSG_RACE_START:   raceStart  = true; break;
			case MSG_FOUL:         foulStatus = true; break;
			case MSG_LEFT_REACT:   leftReact  = true; break;
			case MSG_RIGHT_REACT:  rightReact = true; break;
			case MSG_DISP_ADVANCE: dispAdv    = true; break;
			default: return;
		}
		resetTxState(id);
	}
	void serviceNext();	// out-of-line definition in helpers section below
};

// State & mode machine instances
static stateMachine stm					= {RACE_IDLE, RACE_IDLE, true, false};
static modeMachine mdm					= {MODE_GATEDROP, MODE_GATEDROP};

// timing
static raceTimingData raceTime			= {0, 0, 0};
static raceResultsData raceResults		= {0, 0, false, false};
uint32_t tNow							= 0;			// current time in microseconds	

// countdown 
struct CountDownCtx {
	countdownState state	= CD_IDLE;
	countdownState prev		= CD_IDLE;
	unsigned long timer		= 0;
	unsigned long delay		= 0;		// set in CD_STAGED per mode
};
static CountDownCtx cd;

// racing
static PendingTx pending;
uint8_t foulMask						= 0;			// bitmask of fouls to send

// results
static bool winLightsPend				= false;		// marker if result lights need to display

// error
static bool criticalTxError				= false;		// set on permanent failure of a critical message

// button management
static bool startReleased				= true;
static bool modeReleased				= true;

// Internal helpers (file-local)
static unsigned long elapsedMicros(unsigned long startTime, unsigned long endTime);
countdownState tickCountdownState(raceMode mode, countdownState cdState);
static void handleModeChanges();
static void handleEarlyStarts(unsigned long tn, raceMode mode);
static void handleCountdownGoActions(countdownState cdNow, countdownState cdPrev, long tn);
uint32_t calcReactionTimes(bool foul, uint32_t raceStart, uint32_t carStart);
static void handleTrackTriggers();
static void handleDisplayAdvance();

void startControllerSetup(){
	setupSerial();
	setupButtons();
	setupGates();
	setupLights();

	// Start in idle state.  These variables are declared in raceTypes.h.
	stm.current					= RACE_IDLE;
	stm.target					= RACE_IDLE;
	mdm.current 				= MODE_GATEDROP;
	mdm.target 					= MODE_GATEDROP;
}

void startControllerLoop(){
    /*
     * Coordination model — each transition has one initiator (calls selfTransition,
     * sends MSG_RACE_STATE, waits for ACK) and one follower (receives via rxSerial,
     * calls rxTransition to commit locally without re-broadcasting).
     *
     * Transition             Initiator   Follower
     * IDLE      -> STAGING      SC          FC
     * STAGING   -> IDLE         SC          FC
     * STAGING   -> COUNTDOWN    SC          FC
     * COUNTDOWN -> RACING       SC          FC
     * RACING    -> COMPLETE     FC          SC
     * COMPLETE  -> IDLE         FC          SC
     */
	rxSerial();

	if (criticalTxError) {
		updateLights(LIGHT_FL | LIGHT_FR);		// both red = critical failure, power-cycle required
		return;
	}

	switch(stm.current) {
		case RACE_IDLE:
			if(stm.entry){
				stm.entry		= false;
				cd.state 		= CD_IDLE;
				updateLights(LIGHT_OFF);
				dropGate(gateL);
				dropGate(gateR);
				modeReleased	= true;
				startReleased	= true;

				// Sync check: after a normal race, FC drives COMPLETE->IDLE and SC receives
				// MSG_RACE_STATE(IDLE), setting rx.State = RACE_IDLE. If rx.State is anything
				// else here, FC did not complete its IDLE transition — warn the operator.
				if ((raceState)rx.State != RACE_IDLE) {
					startBlink(LIGHT_FL | LIGHT_FR, 0x00, 3, 250, LIGHT_OFF);	// 3 red blinks, then off
				}
			}
			
			updateBlink();
			handleModeChanges();
			
			if (!blinkState.active){
				if (isStartPressed())	stm.target = RACE_STAGING;		// Start moves to STAGING
			}

			stm.rxTransition((raceState)rx.State);
			if (stm.target != stm.current) stm.selfTransition(stm.target);

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

			updateBlink();

			if(gateStatus.returnActive)	returnGates();					// call this until it returnActive is false

			if (!blinkState.active){
				if (isStartPressed())	stm.target = RACE_COUNTDOWN;	// Start moves to COUNTDOWN
				if (isModePressed())	stm.target = RACE_IDLE;			// Mode returns to IDLE
			}

			if (stm.target != stm.current) stm.selfTransition(stm.target);

			if(stm.exit){
				stm.exit = false;
			}
			break;

		case RACE_COUNTDOWN:
			if(stm.entry){
				stm.entry				= false;
				cd.state 				= CD_STAGED;
				cd.prev 			= cd.state;
			}
			
			tNow = micros();

			handleEarlyStarts(tNow, mdm.current);						// Watch for early starts, drop gates, and log fouls.

			cd.state = tickCountdownState(mdm.current, cd.state);			// Tick the countdown state.
			if (cd.state == CD_GO){
				handleCountdownGoActions(cd.state, cd.prev, tNow);	// When GO is reached, start race and transition state.
			}
			if(cd.state != cd.prev){
				byte cdLights	= buildLightConfig(cd.state, raceResults.leftFoul, raceResults.rightFoul, mdm.current);	// set new light pattern
				updateLights(cdLights);									// update lights only when new cd.state
				cd.prev = cd.state;
			}

			if (stm.target != stm.current) stm.selfTransition(stm.target);

			if(stm.exit){
				stm.exit = false;
			}
			break;
			
		case RACE_RACING:
			if(stm.entry){
				stm.entry						= false;
				raceResults.rightReactUs		= 0;
				raceResults.leftReactUs			= 0;
				foulMask						= 0;
				if (raceResults.leftFoul)  foulMask |= foul_left;
				if (raceResults.rightFoul) foulMask |= foul_right;
				pending.queue(MSG_FOUL);		// always send foul status on entry to RACING
			}

			tNow 							= micros();

			if (mdm.current != MODE_GATEDROP){
				handleTrackTriggers();

				if (!gateStatus.leftUp && raceResults.leftReactUs == 0){
					raceResults.leftReactUs	= calcReactionTimes(raceResults.leftFoul, raceTime.raceStartUs, raceTime.leftStartUs);
				}
				if (!gateStatus.rightUp && raceResults.rightReactUs == 0){
					raceResults.rightReactUs	= calcReactionTimes(raceResults.rightFoul, raceTime.raceStartUs, raceTime.rightStartUs);
				}
			}

			if (!pending.anyPending()) {
				stm.rxTransition((raceState)rx.State);		// only accept COMPLETE once all pending messages sent
			}

			if(stm.exit){
				stm.exit = false;
			}
			break;
			
		case RACE_COMPLETE:
			if(stm.entry){
				stm.entry				= false;
				rx.LeftWin				= false;
				rx.RightWin				= false;
				rx.Tie					= false;
				winLightsPend			= true;
				blinkState.active 		= false;  						// Clear any pending blinks
			}

			handleDisplayAdvance();
			
			if (winLightsPend){ 
				// Determine win light pattern to show winner and start blink
				if(rx.LeftWin)	startBlink(LIGHT_GO | LIGHT_FR, LIGHT_FR, 3, 250, LIGHT_GO | LIGHT_FR);
				if(rx.RightWin)	startBlink(LIGHT_GO | LIGHT_FL, LIGHT_FL, 3, 250, LIGHT_GO | LIGHT_FL);
				if(rx.Tie) 		startBlink(LIGHT_GO, 0x00, 3, 250, LIGHT_GO);
				winLightsPend 			= false;
			}			
				
			if (!winLightsPend && !updateBlink()){				// note: this also executes the updateBlink() function to process blinks
				stm.rxTransition((raceState)rx.State); // wait until all pending messages have been sent until completing transition
			}

			if(stm.exit){
				stm.exit = false;
			}
			break;
			
		case RACE_TEST:  // P2: self-test not yet implemented — falls back to IDLE (see project-status.md)
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

	pending.serviceNext();
}

/* =========================================================================
 *                        RACE_IDLE HELPER FUNCTIONS
 * ========================================================================= */
 static void handleModeChanges(){
 	// Handle mode changes via button press or rxSerial
	if (!blinkState.active){
		if (rx.Mode != mdm.current){
			mdm.rxTransition((raceMode)rx.Mode);								// Handle unsolicited mode changes from rxSerial
		} else {
			if (!isModePressed())		modeReleased	= true;		// button released, ready for next detection
			if (isModePressed() && modeReleased){
				modeReleased			= false;					// don't revisit until released
				mdm.nextMode();										// Select mode to advance to per transition order
			}
		}
		if (mdm.target != mdm.current) mdm.selfTransition(mdm.target);
	}
}

 /* =========================================================================
 *                        RACE_STAGING HELPER FUNCTIONS
 * ========================================================================= */

/* =========================================================================
 *                        RACE_COUNTDOWN HELPER FUNCTIONS
 * ========================================================================= */
static void handleEarlyStarts(unsigned long tn, raceMode mode){
	// Helper function to monitor for early starts during countdown
	// Watch for the triggers (given right mode).  Drop the gate but store a foul.
	if (mode != MODE_GATEDROP){
		if (isLeftPressed() && gateStatus.leftUp){
			raceTime.leftStartUs	= tn;
			raceResults.leftFoul	= true;
			dropGate(gateL);
		}
		if (isRightPressed() && gateStatus.rightUp){
			raceTime.rightStartUs	= tn;
			raceResults.rightFoul	= true;
			dropGate(gateR);
		}
	}
}

static void handleCountdownGoActions(countdownState cdNow, countdownState cdPrev, long tn){
	// On first CD_GO loop: record race start time, arm pending TX, drop gates if gate-drop mode.
	if (cdNow != cdPrev){
		stm.target				= RACE_RACING;
		raceTime.raceStartUs	= tn;
		pending.queue(MSG_RACE_START);

		if (mdm.current == MODE_GATEDROP){
			raceTime.leftStartUs	= tn;
			raceTime.rightStartUs	= tn;
			dropGate(gateL);
			dropGate(gateR);
		}
	}
}

countdownState tickCountdownState(raceMode mode, countdownState cdState){
	// Helper function to manage countdown timing based on race mode
	// This function will handle managing stage delays as well as managing the countdown state
	unsigned long currentTime = millis();
	switch (cdState) {
		case CD_STAGED:
			if (mode == MODE_PRO){
				cd.delay = 400;		// Pro mode uses a 400 ms delay between stages
				cdState = CD_Y1;
			} else {
				cd.delay = 500;		// Standard modes use a 500 ms delay between stages
				cdState = CD_Y3;
			}
			cd.timer = currentTime;
			break;
		case CD_Y3:
			if (currentTime - cd.timer >= cd.delay){
				cdState = CD_Y2;
				cd.timer = currentTime;
			}
			break;
		case CD_Y2:
			if (currentTime - cd.timer >= cd.delay){
				cdState = CD_Y1;
				cd.timer = currentTime;
			}
			break;
		case CD_Y1:
			if (currentTime - cd.timer >= cd.delay){
				cdState = CD_GO;
				cd.timer = currentTime;
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
	// Possible this is unnecessary since unsigned long subtraction wraps automatically
    if (endTime >= startTime) {
        return endTime - startTime;  					// Normal case
    } else {
        return (0xFFFFFFFF - startTime) + endTime + 1;	// Overflow case
    }
}

uint32_t calcReactionTimes(bool foul, uint32_t raceStart, uint32_t carStart){
	// calculate reaction times, gate drop stays at zero
	if (foul){
		return elapsedMicros(raceStart, carStart);		// race time is bigger since they started early
	} else {
		return elapsedMicros(carStart, raceStart);		// normally car time is bigger because it started after race
	}
}

static void handleTrackTriggers(){
	if (isLeftPressed() && gateStatus.leftUp){
		raceTime.leftStartUs	= tNow;
		dropGate(gateL);
		pending.queue(MSG_LEFT_REACT);
	}
	if (isRightPressed() && gateStatus.rightUp){
		raceTime.rightStartUs	= tNow;
		dropGate(gateR);
		pending.queue(MSG_RIGHT_REACT);
	}
}


 /* =========================================================================
 *                        RACE_COMPLETE HELPER FUNCTIONS
 * ========================================================================= */

static void handleDisplayAdvance(){
	if (isStartPressed() && startReleased){
		startReleased		= false;
		pending.queue(MSG_DISP_ADVANCE);
	}
	if (!isStartPressed()) startReleased = true;
}

 /* =========================================================================
 *                        GENERIC HELPER FUNCTIONS
 * ========================================================================= */

void PendingTx::serviceNext() {
	if (raceStart) {
		txStatus s = txRaceStart(0b0001);
		if (s == TX_ACKED) {
			raceStart = false;
			resetTxState(MSG_RACE_START);
		} else if (s == TX_TIMEOUT || s == TX_FAILED) {
			raceStart = false;
			resetTxState(MSG_RACE_START);
			criticalTxError = true;		// raceStart failure: FC sensors may not be armed
		}
		return;
	}
	if (foulStatus) {
		txStatus s = txFoulStatus(foulMask);
		if (s == TX_ACKED || s == TX_TIMEOUT || s == TX_FAILED) {
			if (s != TX_ACKED) criticalTxError = true;	// FC will compute wrong winner
			foulStatus = false;
			resetTxState(MSG_FOUL);
		}
		return;
	}
	if (leftReact) {
		txStatus s = txReactionTime(raceResults.leftReactUs, true);
		if (s == TX_ACKED || s == TX_TIMEOUT || s == TX_FAILED) {
			if (s != TX_ACKED) criticalTxError = true;	// FC will compute wrong car time
			leftReact = false;
			resetTxState(MSG_LEFT_REACT);
		}
		return;
	}
	if (rightReact) {
		txStatus s = txReactionTime(raceResults.rightReactUs, false);
		if (s == TX_ACKED || s == TX_TIMEOUT || s == TX_FAILED) {
			if (s != TX_ACKED) criticalTxError = true;
			rightReact = false;
			resetTxState(MSG_RIGHT_REACT);
		}
		return;
	}
	if (dispAdv) {
		txStatus s = txDisplayAdvance();
		if (s == TX_ACKED || s == TX_TIMEOUT || s == TX_FAILED) {
			dispAdv = false;
			resetTxState(MSG_DISP_ADVANCE);
			// non-critical: operator can press again
		}
		return;
	}
}