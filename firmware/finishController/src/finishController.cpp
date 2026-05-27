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
#include "debug.h"


struct PendingTx {
	bool    winner         = false;
	uint8_t winnerAttempts = 0;
	bool anyPending() const { return winner; }
	void queue(serialMsgID id);
	void checkOutcomes();
};

struct FCTestCtx {
	uint8_t  phase;
	uint8_t  subPhase;
	unsigned long timer;
	uint8_t  failCodes[5];
	uint8_t  failCount;
	bool     scCommReceived;
	uint32_t sensorStartUs;
	bool     leftFrozen;
	bool     rightFrozen;
	uint8_t  dispCodeIdx;
	uint8_t  dispState;

	void reset() {
		phase = 0; subPhase = 0; timer = 0; failCount = 0;
		scCommReceived = false; sensorStartUs = 0;
		leftFrozen = rightFrozen = false;
		dispCodeIdx = dispState = 0;
		memset(failCodes, 0, sizeof(failCodes));
	}
	void addFail(uint8_t c) { if (failCount < 5) failCodes[failCount++] = c; }
	bool elapsed(unsigned long ms) const {
		return (unsigned long)(millis() - timer) >= ms;
	}
	void startTimer() { timer = millis(); }
	void nextPhase(uint8_t p) { phase = p; subPhase = 0; startTimer(); }
};

static bool needReact              = false;
static PendingTx pending;
static FCTestCtx fct;
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
static void displayCarTimes();
static void displayReactionTimes();


// ==================== RACE_TEST SUPPORT ====================
// Display sequence: all-segments (88.888), then countdown 05→01 at 500ms each
struct DispStep { uint32_t valUs; uint16_t holdMs; };
static const DispStep kDispSteps[] = {
	{ 88888000UL, 1000 },  // all segments lit: 88.888
	{ 5000000UL,   500 },  // 05.000
	{ 4000000UL,   500 },  // 04.000
	{ 3000000UL,   500 },  // 03.000
	{ 2000000UL,   500 },  // 02.000
	{ 1000000UL,   500 },  // 01.000
};
static const uint8_t kDispStepCount = sizeof(kDispSteps) / sizeof(kDispSteps[0]);

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
				DBG("[FC] ->IDLE");
				rx.clearHeatEvents();
				disarmSensors();
				heatResult   = {};
				timingInputs = {};
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
				DBG("[FC] ->STAGING");
			}
			stm.rxTransition((raceState)rx.State);
			if(stm.exit){
				stm.exit 			= false;
			}

			break;

		case RACE_COUNTDOWN:
			if(stm.entry){
				stm.entry            = false;
				DBG("[FC] ->COUNTDOWN");
				timingInputs.startUs = 0;
				countdownEntryMs     = millis();
			}

			// SC went silent — sensors never armed, heat is unrecoverable. Abort to IDLE.
			if ((millis() - countdownEntryMs) > 10000UL) {
				DBG("[FC] countdown timeout->forceIdle");
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
				DBG("[FC] ->RACING");
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
				DBG("[FC] ->COMPLETE");
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
				DBG2("[FC] winner mask=", winnerMask);
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
			if (stm.entry) {
				stm.entry = false;
				DBG("[FC] ->RACE_TEST");
				fct.reset();
				fct.scCommReceived = ((raceState)rx.State == RACE_TEST);
				fct.phase = 1;
				fct.startTimer();
			}

			switch (fct.phase) {
				case 1: {  // Display test: 88.888 all-segments, then countdown 05→01
					if (fct.subPhase == 0) {
						updateDisplay(kDispSteps[0].valUs, LANE_LEFT);
						updateDisplay(kDispSteps[0].valUs, LANE_RIGHT);
						fct.startTimer();
						fct.subPhase = 1;
					} else if (fct.elapsed(kDispSteps[fct.subPhase - 1].holdMs)) {
						if (fct.subPhase < kDispStepCount) {
							updateDisplay(kDispSteps[fct.subPhase].valUs, LANE_LEFT);
							updateDisplay(kDispSteps[fct.subPhase].valUs, LANE_RIGHT);
							fct.startTimer();
							fct.subPhase++;
						} else {
							clearDisplay(LANE_LEFT);
							clearDisplay(LANE_RIGHT);
							fct.nextPhase(2);
						}
					}
					break;
				}

				case 2: {  // Sensor test: live elapsed display, freeze on beam-break, 8s timeout
					if (fct.subPhase == 0) {
						fct.sensorStartUs = micros();
						armSensors(fct.sensorStartUs);
						fct.startTimer();
						fct.subPhase = 1;
					}
					if (fct.subPhase == 1) {
						uint32_t elUs = micros() - fct.sensorStartUs;
						unsigned long elMs = (unsigned long)(millis() - fct.timer);

						if (!fct.leftFrozen) {
							if (isLeftFinished()) {
								updateDisplay(getLeftTimeUs(), LANE_LEFT);
								fct.leftFrozen = true;
							} else if (elMs >= 8000) {
								fct.addFail(201);
								clearDisplay(LANE_LEFT);
								fct.leftFrozen = true;
							} else {
								updateDisplay(elUs, LANE_LEFT);
							}
						}
						if (!fct.rightFrozen) {
							if (isRightFinished()) {
								updateDisplay(getRightTimeUs(), LANE_RIGHT);
								fct.rightFrozen = true;
							} else if (elMs >= 8000) {
								fct.addFail(202);
								clearDisplay(LANE_RIGHT);
								fct.rightFrozen = true;
							} else {
								updateDisplay(elUs, LANE_RIGHT);
							}
						}
						if (fct.leftFrozen && fct.rightFrozen) {
							disarmSensors();
							fct.nextPhase(3);
						}
					}
					break;
				}

				case 3: {  // SC comm check: 3s window to confirm SC sent RACE_TEST ping
					if ((raceState)rx.State == RACE_TEST) fct.scCommReceived = true;
					if (fct.elapsed(3000)) {
						if (!fct.scCommReceived) fct.addFail(203);
						fct.nextPhase(4);
					}
					break;
				}

				case 4: {  // Result display — stays here permanently (power-cycle to exit)
					if (fct.failCount == 0) {
						// All clear: 00.000 on both lanes
						if (fct.subPhase == 0) {
							updateDisplay(0UL, LANE_LEFT);
							updateDisplay(0UL, LANE_RIGHT);
							fct.subPhase = 1;
						}
						break;
					}

					// Failure cycle: show each code (2s) → blank (1s) → 88.888 end marker (1s) → repeat
					if (fct.subPhase == 0) {
						updateDisplay((uint32_t)fct.failCodes[0] * 1000UL, LANE_LEFT);
						updateDisplay((uint32_t)fct.failCodes[0] * 1000UL, LANE_RIGHT);
						fct.dispCodeIdx = 0;
						fct.dispState   = 0;
						fct.startTimer();
						fct.subPhase    = 1;
					}

					switch (fct.dispState) {
						case 0:  // Showing failure code for 2s
							if (fct.elapsed(2000)) {
								clearDisplay(LANE_LEFT);
								clearDisplay(LANE_RIGHT);
								fct.dispState = 1;
								fct.startTimer();
							}
							break;
						case 1:  // Blank separator for 1s
							if (fct.elapsed(1000)) {
								fct.dispCodeIdx++;
								if (fct.dispCodeIdx < fct.failCount) {
									updateDisplay((uint32_t)fct.failCodes[fct.dispCodeIdx] * 1000UL, LANE_LEFT);
									updateDisplay((uint32_t)fct.failCodes[fct.dispCodeIdx] * 1000UL, LANE_RIGHT);
									fct.dispState = 0;
								} else {
									updateDisplay(88888000UL, LANE_LEFT);
									updateDisplay(88888000UL, LANE_RIGHT);
									fct.dispState = 2;
								}
								fct.startTimer();
							}
							break;
						case 2:  // End marker (88.888) for 1s, then restart cycle
							if (fct.elapsed(1000)) {
								fct.dispCodeIdx = 0;
								updateDisplay((uint32_t)fct.failCodes[0] * 1000UL, LANE_LEFT);
								updateDisplay((uint32_t)fct.failCodes[0] * 1000UL, LANE_RIGHT);
								fct.dispState = 0;
								fct.startTimer();
							}
							break;
					}
					break;
				}
			}

			if (stm.exit) {
				stm.exit = false;
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
			DBG2("[FC] left finish us=", timingInputs.leftTimeUs);
		} else if (elapsed > config.maxRaceTimeUs) {
			timingInputs.leftTimeUs   = config.maxRaceTimeUs;
			timingInputs.leftDnf      = true;
			timingInputs.leftRecorded = true;
			DBG("[FC] left DNF");
		}
    }

	if (!timingInputs.rightRecorded) {
		if (isRightFinished()) {
			timingInputs.rightTimeUs   = getRightTimeUs();
			timingInputs.rightRecorded = true;
			DBG2("[FC] right finish us=", timingInputs.rightTimeUs);
		} else if (elapsed > config.maxRaceTimeUs) {
			timingInputs.rightTimeUs   = config.maxRaceTimeUs;
			timingInputs.rightDnf      = true;
			timingInputs.rightRecorded = true;
			DBG("[FC] right DNF");
		}
    }
}

void handleRxReaction() {
	if (rx.LeftReactionValid) {
		heatResult.left.reactionTimeUs = (uint32_t)rx.LeftReactionTime;
		rx.LeftReactionValid           = false;
		rx.LeftReactionTime            = 0;
		DBG2("[FC] left react us=", heatResult.left.reactionTimeUs);
	}
	if (rx.RightReactionValid) {
		heatResult.right.reactionTimeUs = (uint32_t)rx.RightReactionTime;
		rx.RightReactionValid           = false;
		rx.RightReactionTime            = 0;
		DBG2("[FC] right react us=", heatResult.right.reactionTimeUs);
	}
	if (rx.LeftFoul) {
		heatResult.left.foul = true;
		rx.LeftFoul          = false;
		DBG("[FC] rx foul: LEFT");
	}
	if (rx.RightFoul) {
		heatResult.right.foul = true;
		rx.RightFoul          = false;
		DBG("[FC] rx foul: RIGHT");
	}
}

/* =========================================================================
 *                        RACE_COMPLETE HELPER FUNCTIONS
 * ========================================================================= */

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
