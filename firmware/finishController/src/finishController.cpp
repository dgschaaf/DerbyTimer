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
#include "raceTypes.h"
#include "serialComm.h"
#include "stateMachine.h"
#include "finishController.h"
#include "display.h"
#include "sensors.h"

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

struct PendingTx {
	bool winner = false;

	bool anyPending() const { return winner; }
	void queue(serialMsgID id) {
		if (id == MSG_WINNER) winner = true;
		else return;
		resetTxState(id);
	}
	void serviceNext();
};

// State flags instance
bool needReact 			= false;		// Reaction time is needed
static PendingTx pending;
static bool criticalTxError = false;		// unused in FC currently; reserved for future critical messages


// Static instances for left and right lanes; lifetime extends over loops.
static raceResults leftResults	= {true, false, false, 0, 0, 0};
static raceResults rightResults	= {false, false, false, 0, 0, 0};
static raceTimingData race		= {0, 0, 0, false, false};

// State machine instance
static stateMachine stm			= {RACE_IDLE, RACE_IDLE, true, false};
static raceMode currentMode;

// Internal helpers (file-local)
static void handleSensors();
static void handleRxReaction();
static void computeRaceTimes();
static void displayCarTimes();
static void displayReactionTimes();


void finishControllerSetup() {
	setupSerial();
	setupSensors();
	setupDisplay();	

	// Start in idle state.  These variables are declared in raceTypes.h.
    stm.current					= RACE_IDLE;
    stm.target					= RACE_IDLE;
    currentMode 				= MODE_GATEDROP;
}

void finishControllerLoop() {
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
		clearDisplay(true);		// blank both displays
		clearDisplay(false);
		return;
	}

	switch(stm.current) {
		case RACE_IDLE:
			if(stm.entry){
				stm.entry 			= false;
				clearDisplay(true);				// clear display (left)
				clearDisplay(false);			// clear display (right))
			}

			if (rx.Mode != currentMode){
				currentMode 		= (raceMode)rx.Mode;	// update mode from serial, source will validate
				// notifyBLEMode(currentMode);	// Future - notify mode change over BLE
			}
			stm.rxTransition((raceState)rx.State);			// transitions state if received via serial
			if(stm.exit){
				stm.exit 			= false;
			}

			break;
			
		case RACE_STAGING:
			if(stm.entry){
				stm.entry 			= false;
			}
			stm.rxTransition((raceState)rx.State);			// transitions state if received via serial
			if(stm.exit){
				stm.exit 			= false;
			}

			break;
			
		case RACE_COUNTDOWN:
			if(stm.entry){
				stm.entry 			= false;
				rx.RaceStart			= false;
				race.raceStartUs	= 0;
			}

			if (rx.RaceStart && (race.raceStartUs == 0)) {
				race.raceStartUs	= micros();
				armSensors(race.raceStartUs);
			}
			stm.rxTransition((raceState)rx.State);						// transitions state if received via serial	
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
				rx.LeftReactionValid		= false;
				rx.RightReactionValid	= false;
				rx.LeftReactionTime		= 0;
				rx.RightReactionTime	= 0;
				rx.LeftFoul				= false;
				rx.RightFoul			= false;
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
			if (stm.target != stm.current) stm.selfTransition(stm.target);

			if(stm.exit){
				stm.exit 				= false;
				disarmSensors();
			}
			break;
			
		case RACE_COMPLETE:
			if(stm.entry){
				needReact				= false;
				rx.DisplayAdvanceFlag	= false;
				computeRaceTimes();
				displayCarTimes();
				pending.queue(MSG_WINNER);		// transmit winner once per race
				stm.entry				= false;
			}

			if(rx.DisplayAdvanceFlag) {
				if(needReact){
					displayReactionTimes();
				} else if (!pending.anyPending()) {
					stm.target		= RACE_IDLE;	// only transition once winner TX is done
				}
				rx.DisplayAdvanceFlag	= false;
			}
			if (stm.target != stm.current) stm.selfTransition(stm.target);

			if(stm.exit){
				stm.exit 				= false;
				// carID, left, foul, winner, carTimeUs, raceTimeUs, reactionTimeUs
				leftResults		= {true,	false,	false,	0,	0,	0};		// reset left results struct
				rightResults	= {false,	false,	false,	0,	0,	0};		// reset right results struct
			}
			break;
			
		case RACE_TEST:
			// P2: self-test not yet implemented — falls back to IDLE (see project-status.md)
			if(stm.entry){
				stm.target 		= RACE_IDLE;
				stm.entry 		= false;
			}

			if (stm.target != stm.current) stm.selfTransition(stm.target);

			if(stm.exit){
				stm.exit 		= false;
			}
			break;
	}

	pending.serviceNext();
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
	if (rx.LeftReactionValid) {
		leftResults.reactionTimeUs 	= (uint32_t)rx.LeftReactionTime;
		rx.LeftReactionValid		= false;
		rx.LeftReactionTime			= 0;
	}
	if (rx.RightReactionValid) {
		rightResults.reactionTimeUs	= (uint32_t)rx.RightReactionTime;
		rx.RightReactionValid		= false;
		rx.RightReactionTime		= 0;
	}
	if (rx.LeftFoul) {
		leftResults.foul			= true;
		rx.LeftFoul					= false;	// reset flag
	}
	if (rx.RightFoul) {
		rightResults.foul			= true;
		rx.RightFoul					= false;	// reset flag
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
void PendingTx::serviceNext() {
	if (winner) {
		uint8_t winnerMask = 0;
		if (leftResults.winner)  winnerMask |= winner_leftWin;
		if (rightResults.winner) winnerMask |= winner_rightWin;
		if (!leftResults.winner && !rightResults.winner) winnerMask |= winner_tie;

		txStatus s = txWinner(winnerMask);
		if (s == TX_ACKED || s == TX_TIMEOUT || s == TX_FAILED) {
			winner = false;
			resetTxState(MSG_WINNER);
			// non-critical: finish times still display even if winner TX fails
		}
	}
}