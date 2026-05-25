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

struct HeatLaneResult {
	bool		foul;
	bool		winner;
	uint32_t	raceTimeUs;
	uint32_t	reactionTimeUs;
	uint32_t	carTimeUs;
};

struct HeatResults {
	HeatLaneResult left;
	HeatLaneResult right;
};

struct HeatTimingData {
	uint32_t startUs;
	uint32_t leftTimeUs;
	uint32_t rightTimeUs;
	bool     leftRecorded;
	bool     rightRecorded;
};

struct PendingTx {
	bool winner = false;

	bool anyPending() const { return winner; }
	void queue(serialMsgID id);
	void checkOutcomes();
};

static bool needReact			= false;
static PendingTx pending;
static bool criticalTxError		= false;		// reserved for future critical messages

static HeatResults    heatResult	= {};
static HeatTimingData heat			= {};

// State machine instance
static stateMachine stm			= {RACE_IDLE, RACE_IDLE, true, false};
static raceMode currentMode;

// Internal helpers (file-local)
static void handleSensors();
static void handleRxReaction();
static HeatResults computeHeatResults(
	uint32_t leftRaceTimeUs, uint32_t rightRaceTimeUs,
	bool leftFoul, bool rightFoul,
	uint32_t leftReactionUs, uint32_t rightReactionUs);
static void displayCarTimes();
static void displayReactionTimes();


void finishControllerSetup() {
	setupSerial();
	setupSensors();
	setupDisplay();

	// Start in idle state.  These variables are declared in raceTypes.h.
    stm.current				= RACE_IDLE;
    stm.target				= RACE_IDLE;
    currentMode 			= MODE_GATEDROP;
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

	if (rx.lastErrorCode != err_NULL && !criticalTxError) {
		criticalTxError = true;
	}

	if (criticalTxError) {
		static unsigned long lastBlink = 0;
		static bool blinkOn = false;
		unsigned long now = millis();
		if (now - lastBlink >= 500) {
			lastBlink = now;
			blinkOn = !blinkOn;
			if (blinkOn) {
				updateDisplay(88888000UL, LANE_LEFT);
				updateDisplay(88888000UL, LANE_RIGHT);
			} else {
				clearDisplay(LANE_LEFT);
				clearDisplay(LANE_RIGHT);
			}
		}
		return;
	}

	switch(stm.current) {
		case RACE_IDLE:
			if(stm.entry){
				stm.entry 			= false;
				clearDisplay(LANE_LEFT);
				clearDisplay(LANE_RIGHT);
			}

			if (rx.Mode != currentMode){
				currentMode 		= (raceMode)rx.Mode;
				// future: notify mode change over BLE
			}
			stm.rxTransition((raceState)rx.State);
			if(stm.exit){
				stm.exit 			= false;
			}

			break;

		case RACE_STAGING:
			if(stm.entry){
				stm.entry 			= false;
			}
			stm.rxTransition((raceState)rx.State);
			if(stm.exit){
				stm.exit 			= false;
			}

			break;

		case RACE_COUNTDOWN:
			if(stm.entry){
				stm.entry 			= false;
				rx.RaceStart		= false;
				heat.startUs		= 0;
			}

			if (rx.RaceStart && (heat.startUs == 0)) {
				heat.startUs		= micros();
				armSensors(heat.startUs);
			}
			stm.rxTransition((raceState)rx.State);
			if(stm.exit){
				stm.exit 			= false;
			}

			break;

		case RACE_RACING:
			if(stm.entry){
				heat.leftRecorded		= false;
				heat.rightRecorded		= false;
				heat.leftTimeUs			= 0;
				heat.rightTimeUs		= 0;
				rx.LeftReactionValid	= false;
				rx.RightReactionValid	= false;
				rx.LeftReactionTime		= 0;
				rx.RightReactionTime	= 0;
				rx.LeftFoul				= false;
				rx.RightFoul			= false;
				stm.entry 				= false;
				// Only arm if not already armed from COUNTDOWN state
				if (heat.startUs == 0) {
					heat.startUs		= micros();
					armSensors(heat.startUs);
				}
			}

			handleSensors();
			handleRxReaction();

			if (heat.leftRecorded && heat.rightRecorded) {
				stm.target	= RACE_COMPLETE;
			}
			if (stm.target != stm.current) stm.selfTransition(stm.target);

			if(stm.exit){
				stm.exit 				= false;
				disarmSensors();
			}
			break;

		case RACE_COMPLETE:
			if(stm.entry){
				needReact				= (currentMode == MODE_REACTION || currentMode == MODE_PRO);
				rx.DisplayAdvanceFlag	= false;
				heatResult = computeHeatResults(
					heat.leftTimeUs,              heat.rightTimeUs,
					heatResult.left.foul,         heatResult.right.foul,
					heatResult.left.reactionTimeUs, heatResult.right.reactionTimeUs
				);
				displayCarTimes();
				pending.queue(MSG_WINNER);
				stm.entry				= false;
			}

			if(rx.DisplayAdvanceFlag) {
				if(needReact){
					displayReactionTimes();
					needReact			= false;
				} else if (!pending.anyPending()) {
					stm.target		= RACE_IDLE;
				}
				rx.DisplayAdvanceFlag	= false;
			}
			if (stm.target != stm.current) stm.selfTransition(stm.target);

			if(stm.exit){
				stm.exit 			= false;
				heatResult			= {};
				heat				= {};
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

	txService();
	pending.checkOutcomes();
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
	uint32_t now     = micros();
	uint32_t elapsed = now - heat.startUs;

    if (!heat.leftRecorded) {
		if (isLeftFinished()) {
			heat.leftTimeUs   = getLeftTimeUs();
			heat.leftRecorded = true;
		} else if (elapsed > config.maxRaceTimeUs) {
			heat.leftTimeUs   = config.maxRaceTimeUs;
			heat.leftRecorded = true;
		}
    }

	if (!heat.rightRecorded) {
		if (isRightFinished()) {
			heat.rightTimeUs   = getRightTimeUs();
			heat.rightRecorded = true;
		} else if (elapsed > config.maxRaceTimeUs) {
			heat.rightTimeUs   = config.maxRaceTimeUs;
			heat.rightRecorded = true;
		}
    }
}

void handleRxReaction() {
	if (rx.LeftReactionValid) {
		heatResult.left.reactionTimeUs = (uint32_t)rx.LeftReactionTime;
		rx.LeftReactionValid           = false;
		rx.LeftReactionTime            = 0;
	}
	if (rx.RightReactionValid) {
		heatResult.right.reactionTimeUs = (uint32_t)rx.RightReactionTime;
		rx.RightReactionValid           = false;
		rx.RightReactionTime            = 0;
	}
	if (rx.LeftFoul) {
		heatResult.left.foul = true;
		rx.LeftFoul          = false;
	}
	if (rx.RightFoul) {
		heatResult.right.foul = true;
		rx.RightFoul          = false;
	}
}

/* =========================================================================
 *                        RACE_COMPLETE HELPER FUNCTIONS
 * ========================================================================= */
static HeatResults computeHeatResults(
	uint32_t leftRaceTimeUs,  uint32_t rightRaceTimeUs,
	bool     leftFoul,        bool     rightFoul,
	uint32_t leftReactionUs,  uint32_t rightReactionUs
) {
	HeatResults r = {};
	r.left.foul            = leftFoul;
	r.right.foul           = rightFoul;
	r.left.raceTimeUs      = leftRaceTimeUs;
	r.right.raceTimeUs     = rightRaceTimeUs;
	r.left.reactionTimeUs  = leftReactionUs;
	r.right.reactionTimeUs = rightReactionUs;

	// No foul: carTime = raceTime - reactionTime (gate-drop to finish)
	// Foul:    carTime = raceTime + reactionTime (gate dropped before GO, so car was rolling longer)
	r.left.carTimeUs  = leftFoul  ? leftRaceTimeUs  + leftReactionUs
	                              : leftRaceTimeUs  - leftReactionUs;
	r.right.carTimeUs = rightFoul ? rightRaceTimeUs + rightReactionUs
	                              : rightRaceTimeUs - rightReactionUs;

	r.left.winner  = !leftFoul  && (rightFoul || r.left.carTimeUs  < r.right.carTimeUs);
	r.right.winner = !rightFoul && (leftFoul  || r.right.carTimeUs < r.left.carTimeUs);

	return r;
}

static void displayCarTimes() {
	if (heatResult.left.foul)  clearDisplay(LANE_LEFT);
	else updateDisplay(heatResult.left.carTimeUs, LANE_LEFT);

	if (heatResult.right.foul) clearDisplay(LANE_RIGHT);
	else updateDisplay(heatResult.right.carTimeUs, LANE_RIGHT);
}

static void displayReactionTimes() {
	if (heatResult.left.foul)  clearDisplay(LANE_LEFT);
	else updateDisplay(heatResult.left.reactionTimeUs, LANE_LEFT);

	if (heatResult.right.foul) clearDisplay(LANE_RIGHT);
	else updateDisplay(heatResult.right.reactionTimeUs, LANE_RIGHT);
}

/* =========================================================================
 *                        GENERIC HELPER FUNCTIONS
 * ========================================================================= */
void PendingTx::queue(serialMsgID id) {
	if (id == MSG_WINNER) {
		uint8_t winnerMask = 0;
		if (heatResult.left.winner)  winnerMask |= winner_leftWin;
		if (heatResult.right.winner) winnerMask |= winner_rightWin;
		if (!heatResult.left.winner && !heatResult.right.winner) winnerMask |= winner_tie;
		if (txWinner(winnerMask)) winner = true;
	}
}

void PendingTx::checkOutcomes() {
	if (winner) {
		txStatus s = txStatusOf(MSG_WINNER);
		if (s == TX_ACKED || s == TX_TIMEOUT || s == TX_FAILED) {
			winner = false;
			// non-critical: finish times still display even if winner TX fails
		}
	}
}
