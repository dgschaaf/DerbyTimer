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

struct LaneResult {
	bool		foul;
	bool		dnf;
	bool		winner;
	uint32_t	raceTimeUs;
	uint32_t	reactionTimeUs;
	uint32_t	carTimeUs;
};

struct HeatResults {
	LaneResult left;
	LaneResult right;
};

struct TimingInputs {
	uint32_t startUs;
	uint32_t leftTimeUs;
	uint32_t rightTimeUs;
	bool     leftRecorded;
	bool     rightRecorded;
	bool     leftDnf;
	bool     rightDnf;
};

struct PendingTx {
	bool    winner         = false;
	uint8_t winnerAttempts = 0;
	bool anyPending() const { return winner; }
	void queue(serialMsgID id);
	void checkOutcomes();
};

static bool needReact              = false;
static PendingTx pending;
static uint8_t winnerMask          = 0;
static bool criticalTxError        = false;
static unsigned long countdownEntryMs = 0;   // P1-11: SC-silent timeout anchor

static HeatResults    heatResult	= {};
static TimingInputs timingInputs	= {};

// State machine instance
static stateMachine stm			= {RACE_IDLE, RACE_IDLE, true, false};
static raceMode currentMode;

// Internal helpers (file-local)
static void handleSensors();
static void handleRxReaction();
static void computeHeatResults(HeatResults& result, const TimingInputs& timing);
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

	if (rx.lastErrorCode != err_NULL) {
		rx.lastErrorCode = err_NULL;
		stm.forceIdle();
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
				stm.entry = false;
				rx.clearHeatEvents();
				disarmSensors();
				clearDisplay(LANE_LEFT);
				clearDisplay(LANE_RIGHT);
			}

			if (rx.Mode != (uint8_t)currentMode && rx.Mode < MODE_COUNT) {
				currentMode = (raceMode)rx.Mode;
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
				stm.entry            = false;
				timingInputs.startUs = 0;
				countdownEntryMs     = millis();
			}

			// SC went silent — sensors never armed, heat is unrecoverable. Abort to IDLE.
			if ((millis() - countdownEntryMs) > 10000UL) {
				stm.forceIdle();
				break;
			}

			if (rx.RaceStart && (timingInputs.startUs == 0)) {
				timingInputs.startUs = micros();
				armSensors(timingInputs.startUs);
			}
			stm.rxTransition((raceState)rx.State);
			if(stm.exit){
				stm.exit 			= false;
			}

			break;

		case RACE_RACING:
			if(stm.entry){
				timingInputs.leftRecorded		= false;
				timingInputs.rightRecorded		= false;
				timingInputs.leftTimeUs			= 0;
				timingInputs.rightTimeUs		= 0;
				timingInputs.leftDnf			= false;
				timingInputs.rightDnf			= false;
				stm.entry 				= false;
				// Only arm if not already armed from COUNTDOWN state
				if (timingInputs.startUs == 0) {
					timingInputs.startUs		= micros();
					armSensors(timingInputs.startUs);
				}
			}

			handleSensors();
			handleRxReaction();

			if (timingInputs.leftRecorded && timingInputs.rightRecorded) {
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
				computeHeatResults(heatResult, timingInputs);

				// Warn if reaction times were expected but never received (lost serial TX).
				// Car times will equal race times — incorrect but best-effort; result is still displayed.
				if (currentMode == MODE_REACTION || currentMode == MODE_PRO) {
					bool leftNeedsReact  = !heatResult.left.foul  && !heatResult.left.dnf;
					bool rightNeedsReact = !heatResult.right.foul && !heatResult.right.dnf;
					if ((leftNeedsReact  && heatResult.left.reactionTimeUs  == 0) ||
					    (rightNeedsReact && heatResult.right.reactionTimeUs == 0)) {
						txError(err_STATE_TX_TIMEOUT);
					}
				}

				displayCarTimes();
				bool leftValid  = !heatResult.left.foul  && !heatResult.left.dnf;
				bool rightValid = !heatResult.right.foul && !heatResult.right.dnf;
				needReact = (currentMode == MODE_REACTION || currentMode == MODE_PRO)
				          && (leftValid || rightValid);
				winnerMask = 0;
				if (!leftValid && !rightValid) {
					winnerMask = winner_noResult;
				} else {
					if (heatResult.left.winner)  winnerMask |= winner_leftWin;
					if (heatResult.right.winner) winnerMask |= winner_rightWin;
					if (!heatResult.left.winner && !heatResult.right.winner) winnerMask |= winner_tie;
				}
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
				timingInputs		= {};
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
	uint32_t elapsed = now - timingInputs.startUs;

    if (!timingInputs.leftRecorded) {
		if (isLeftFinished()) {
			timingInputs.leftTimeUs   = getLeftTimeUs();
			timingInputs.leftRecorded = true;
		} else if (elapsed > config.maxRaceTimeUs) {
			timingInputs.leftTimeUs   = config.maxRaceTimeUs;
			timingInputs.leftDnf      = true;
			timingInputs.leftRecorded = true;
		}
    }

	if (!timingInputs.rightRecorded) {
		if (isRightFinished()) {
			timingInputs.rightTimeUs   = getRightTimeUs();
			timingInputs.rightRecorded = true;
		} else if (elapsed > config.maxRaceTimeUs) {
			timingInputs.rightTimeUs   = config.maxRaceTimeUs;
			timingInputs.rightDnf      = true;
			timingInputs.rightRecorded = true;
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
static void computeHeatResults(HeatResults& result, const TimingInputs& timing) {
	// foul and reactionTimeUs already populated by handleRxReaction() during RACING
	result.left.dnf         = timing.leftDnf;
	result.right.dnf        = timing.rightDnf;
	result.left.raceTimeUs  = timing.leftTimeUs;
	result.right.raceTimeUs = timing.rightTimeUs;

	// No foul: carTime = raceTime - reactionTime (gate-drop to finish)
	// Foul:    carTime = raceTime + reactionTime (gate dropped before GO, so car was rolling longer)
	// DNF:     carTime is computed but meaningless -- winner logic uses dnf flag, display blanks it
	result.left.carTimeUs  = result.left.foul
	                       ? timing.leftTimeUs  + result.left.reactionTimeUs
	                       : timing.leftTimeUs  - result.left.reactionTimeUs;
	result.right.carTimeUs = result.right.foul
	                       ? timing.rightTimeUs + result.right.reactionTimeUs
	                       : timing.rightTimeUs - result.right.reactionTimeUs;

	bool leftValid  = !result.left.foul  && !result.left.dnf;
	bool rightValid = !result.right.foul && !result.right.dnf;
	result.left.winner  = leftValid  && (!rightValid || result.left.carTimeUs  < result.right.carTimeUs);
	result.right.winner = rightValid && (!leftValid  || result.right.carTimeUs < result.left.carTimeUs);
}

static void displayCarTimes() {
	if (heatResult.left.foul  || heatResult.left.dnf)  clearDisplay(LANE_LEFT);
	else updateDisplay(heatResult.left.carTimeUs, LANE_LEFT);

	if (heatResult.right.foul || heatResult.right.dnf) clearDisplay(LANE_RIGHT);
	else updateDisplay(heatResult.right.carTimeUs, LANE_RIGHT);
}

static void displayReactionTimes() {
	if (heatResult.left.foul  || heatResult.left.dnf)  clearDisplay(LANE_LEFT);
	else updateDisplay(heatResult.left.reactionTimeUs, LANE_LEFT);

	if (heatResult.right.foul || heatResult.right.dnf) clearDisplay(LANE_RIGHT);
	else updateDisplay(heatResult.right.reactionTimeUs, LANE_RIGHT);
}

/* =========================================================================
 *                        GENERIC HELPER FUNCTIONS
 * ========================================================================= */
void PendingTx::queue(serialMsgID id) {
	switch(id) {
		case MSG_WINNER:
			if (txWinner(winnerMask)) winner = true;
			break;
		default:
			break;
	}
}

void PendingTx::checkOutcomes() {
	if (winner) {
		txStatus s = txStatusOf(MSG_WINNER);
		if (s == TX_ACKED) {
			winner = false;
			winnerAttempts = 0;
		} else if (s == TX_TIMEOUT || s == TX_FAILED) {
			if (winnerAttempts < 1) {
				winnerAttempts++;
				txWinner(winnerMask);   // one re-send attempt; finish times still display if this also fails
			} else {
				winner = false;
				winnerAttempts = 0;
			}
		}
	}
}
