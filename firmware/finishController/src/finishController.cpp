/*
 * Pinewood Derby Track Finish Controller
 * Version: 1.0
 * Author: Darren Schaaf
 * Date December 2025
 * Compile: Arduino IDE 1.8+ or PlatformIO
 * Board: Arduino Nano 33 BLE
 * Libraries Required:
 */

#include <Arduino.h>
#include "finishController.h"
#include "display.h"
#include "sensors.h"
#include "serialComm.h"
#include "globals.h"

struct stateMachine {
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

        return;  // Already in target state
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

// Results structure for a lane.  Times are stored in microseconds
struct raceResults {
	bool		left;			// indicates track left(true) or right(false);
    bool		foul;			// whether a foul occurred (false start)
    bool		winner;			// true if this lane won the race
    uint32_t	carTimeUs;		// computed car time including or excluding reaction
    uint32_t	raceTimeUs;		// raw finish time from sensors
    uint32_t	reactionTimeUs;	// reaction time measured at start
};

struct raceTimingData {
    uint32_t raceStartUs;
    uint32_t leftTimeUs;
    uint32_t rightTimeUs;
    bool leftRecorded;
    bool rightRecorded;
};

// State flags instance
bool needReact 			= false;		// Reaction time is needed
bool txWinPending		= false;		// Winner transmission is pending


// Static instances for left and right lanes; lifetime extends over loops.
static raceResults leftResults	= {true, false, false, 0, 0, 0};
static raceResults rightResults	= {false, false, false, 0, 0, 0};
static raceTimingData race		= {0, 0, 0, false, false};

// State machine instance
static stateMachine stm			= {RACE_IDLE, RACE_IDLE, true, false};
static raceMode currentMode;


void finishControllerSetup() {
	setupSerial();
	setupSensors();
	setupDisplay();	

	// Start in idle state.  These variables are declared in globals.h.
    stm.current					= RACE_IDLE;
    stm.target					= RACE_IDLE;
    currentMode 				= MODE_GATEDROP;
}

// Internal helpers (file-local)
static void handleSensors();
static void handleRxReaction();
static void computeRaceTimes();
static void transmitWinnerToSC();
static void displayCarTimes();
static void displayReactionTimes();


void finishControllerLoop() {
	rxSerial();
	switch(stm.current) {
		case RACE_IDLE:
			if(stm.entry){
				stm.entry 			= false;
				clearDisplay(true);				// clear display (left)
				clearDisplay(false);			// clear display (right))
			}

			if (rxMode != currentMode){
				currentMode 		= rxMode;	// update mode from serial, source will validate
				// notifyBLEMode(currentMode);	// Future - notify mode change over BLE
			}
			stm.rxTransition(rxState);			// transitions state if received via serial
			if(stm.exit){
				stm.exit 			= false;
			}

			break;
			
		case RACE_STAGING:
			if(stm.entry){
				stm.entry 			= false;
			}
			stm.rxTransition(rxState);			// transitions state if received via serial
			if(stm.exit){
				stm.exit 			= false;
			}

			break;
			
		case RACE_COUNTDOWN:
			if(stm.entry){
				stm.entry 			= false;
				rxRaceStart			= false;
				race.raceStartUs	= 0;
			}

			if (rxRaceStart && (race.raceStartUs == 0)) {
				race.raceStartUs	= micros();
				armSensors(race.raceStartUs);
			}
			stm.rxTransition(rxState);						// transitions state if received via serial	
			if(stm.exit){
				stm.exit 			= false;		
			}

			break;
			
		case RACE_RACING:
			if(stm.entry){
				// Reset recording flags and times
				race.leftRecorded		= false;
				race.rightRecorded		= false;
				race.leftTimeUs			= 0;
				race.rightTimeUs		= 0;
				rxRightReactionTime		= -1;
				rxLeftReactionTime		= -1;
				rxLeftFoul				= false;
				rxRightFoul				= false;
				stm.entry 				= false;
				// Only arm if not already armed from COUNTDOWN state
				if (race.raceStartUs	== 0){
					race.raceStartUs 	= micros();
					armSensors(race.raceStartUs);
				}
			}

			handleSensors();					// check for interrupt and record finish time
			handleRxReaction();					// store reactio and foul from rxSerial

			if (race.leftRecorded && race.rightRecorded) {
				stm.target	= RACE_COMPLETE;	// initiate state transition when both sensors recorded
			}
			stm.selfTransition(stm.target);			// transitions state if updated target
			
			if(stm.exit){
				stm.exit 				= false;	
				disarmSensors();	
			}
			break;
			
		case RACE_COMPLETE:
			if(stm.entry){
				needReact				= false;	// clear flag for safety
				rxDisplayAdvanceFlag	= false;	// clear flag for safety
				txWinPending			= true;		// set winner transmission flag
				computeRaceTimes();					// calculate and compile race times, reaction times, and winner
				displayCarTimes();					// push car times to display
				stm.entry 				= false;	// done with stm.entry tasks
			}

			if (txWinPending){
				transmitWinnerToSC();				// send winner over serial to startController
			}

			if(rxDisplayAdvanceFlag) {
				// When startControll signals to advance display (start trigger)
				if(needReact){
					displayReactionTimes();
				} else {
					stm.target			= RACE_IDLE;
				}
				rxDisplayAdvanceFlag	= false;
			}
			stm.selfTransition(stm.target);				// transitions state if updated target

			if(stm.exit){
				stm.exit 				= false;
				// carID, left, foul, winner, carTimeUs, raceTimeUs, reactionTimeUs
				leftResults		= {true,	false,	false,	0,	0,	0};		// reset left results struct
				rightResults	= {false,	false,	false,	0,	0,	0};		// reset right results struct
			}
			break;
			
		case RACE_TEST:
			// currently unused, just transition back to idle
			if(stm.entry){
				stm.target 		= RACE_IDLE;
				stm.entry 		= false;
			}
			
			stm.selfTransition(stm.target);				// transitions state if updated target

			if(stm.exit){
				stm.exit 		= false;
			}
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

/* =========================================================================
 *                        RACE_RACING HELPER FUNCTIONS
 * ========================================================================= */
void handleSensors() {
	uint32_t now = micros();
	uint32_t elapsed = now - race.raceStartUs;
	
    if (!race.leftRecorded) {
		// If the left sensor has finished, save finish time.
		if (isLeftFinished()) {
			race.leftTimeUs  	= getLeftTimeUs();
			race.leftRecorded 	= true;
		// If max race time exceeded, save finish time.
		} else if (elapsed > config.maxRaceTimeUs) {
			race.leftTimeUs 		= config.maxRaceTimeUs;
			race.leftRecorded 	= true;
		}
    }
	
	if (!race.rightRecorded) {
		// If the left sensor has finished, save finish time.
		if (isRightFinished()) {
			race.rightTimeUs  	= getRightTimeUs();
			race.rightRecorded 	= true;
		// If max race time exceeded, save finish time.
		} else if (elapsed > config.maxRaceTimeUs) {
			race.rightTimeUs 	= config.maxRaceTimeUs;
			race.rightRecorded 	= true;
		}
    }
}

void handleRxReaction() {
	if (rxLeftReactionTime >= 0) {
		leftResults.reactionTimeUs 	= (uint32_t)rxLeftReactionTime;
		rxLeftReactionTime 			= -1;		// reset flag
	}
	if (rxRightReactionTime >= 0) {
		rightResults.reactionTimeUs	= (uint32_t)rxRightReactionTime;	
		rxRightReactionTime 		= -1;		// reset flag
	}
	if (rxLeftFoul) {
		leftResults.foul			= true;
		rxLeftFoul					= false;	// reset flag
	}
	if (rxRightFoul) {
		rightResults.foul			= true;
		rxRightFoul					= false;	// reset flag
	}
	
}

/* =========================================================================
 *                        RACE_COMPLETE HELPER FUNCTIONS
 * ========================================================================= */
void computeRaceTimes() {
	// race time is the raw time from GO to FINISH
	leftResults.raceTimeUs	= race.leftTimeUs;
	rightResults.raceTimeUs	= race.rightTimeUs;
	
	// carTime is raceTime with reactionTime
	// foul indicates addition (trigger before GO) so multiply by +1
	// no foul indicates subtraction (trigger after GO) so multiply by -1
	leftResults.carTimeUs  = leftResults.raceTimeUs  + (leftResults.foul  ? +1 : -1) * leftResults.reactionTimeUs;
	rightResults.carTimeUs = rightResults.raceTimeUs + (rightResults.foul ? +1 : -1) * rightResults.reactionTimeUs;
	
	// cannot win if foul, if both foul no winner (tie).  If no foul fastest carTime wins
	leftResults.winner  = !leftResults.foul  && (rightResults.foul || (leftResults.carTimeUs  < rightResults.carTimeUs));		// winner if no foul AND (other track fouls OR faster time)
	rightResults.winner = !rightResults.foul && (leftResults.foul  || (rightResults.carTimeUs < leftResults.carTimeUs));		// winner if no foul AND (other track fouls OR faster time)
}

void transmitWinnerToSC(){
	// Determine the winner mask: bit0=L, bit1=R, bit2=tie.
	uint8_t winnerMask = 0;
	if (leftResults.winner)  winnerMask |= 0b0001;
	if (rightResults.winner) winnerMask |= 0b0010;
	if (!leftResults.winner && !rightResults.winner) {
		// Neither flagged winner – treat as tie.
		winnerMask |= 0b0100;
	}
	txStatus win = txWinner(winnerMask);
	switch (win) {
		case TX_ACKED:										
			txWinPending = false;						// winner transmission no longer pending
			resetTxState(MSG_WINNER);
			break;
		case TX_TIMEOUT:
		case TX_FAILED:
			txWinPending = false;						// winner transmission failed
			resetTxState(MSG_WINNER);					// reset transmit message
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

static void displayCarTimes() {	
	updateDisplay(leftResults.carTimeUs, true);
	updateDisplay(rightResults.carTimeUs, false);
	
	if(currentMode != MODE_GATEDROP){
		needReact			= true; 		// set flag to display reaction times next
	}
}

static void displayReactionTimes() {	
	updateDisplay(leftResults.reactionTimeUs, true);
	updateDisplay(rightResults.reactionTimeUs, false);
}

/* =========================================================================
 *                        GENERIC HELPER FUNCTIONS
 * ========================================================================= */