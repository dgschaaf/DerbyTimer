/*
 * Pinewood Derby Track Finish Controller
 * Version: 1.0
 * Author: Darren Schaaf
 * Compile: arduino-cli or PlatformIO (see README "Deployment")
 * Board: Arduino Nano 33 BLE (nRF52840)
 *
 * Owns the finish-line half of the track: ISR optical sensors, car-time
 * computation, and the 5-digit displays. Follows the SC through
 * IDLE->STAGING->COUNTDOWN->RACING; initiates RACING->COMPLETE->IDLE once
 * both lanes finish (or DNF). Protocol runs on Serial1 (D0/D1); USB Serial
 * is free for DERBY_DEBUG output. Structure: finishControllerLoop()
 * dispatches to one handleXxx() function per race state.
 */

#include <Arduino.h>
#include "raceTypes.h"
#include "serialComm.h"
#include "outbox.h"
#include "stateMachine.h"
#include "finishController.h"
#include "display.h"
#include "sensors.h"
#include "debug.h"


// The FC's one heat-event delivery rule (its row of the ADR-0004 abort
// criteria): a lost MSG_WINNER only costs the SC's win-light animation --
// the finish times are already on the lane displays -- so it gets one
// automatic resend, then is dropped silently. Tracking starts at COMPLETE
// entry; COMPLETE refuses to advance to IDLE while outbox.anyPending().
static const OutboxEntry kOutboxTable[] = {
	{ MSG_WINNER, OutboxPolicy::RETRY_ONCE, err_NULL },
};
static Outbox outbox = { kOutboxTable, sizeof(kOutboxTable) / sizeof(kOutboxTable[0]) };

// Workbook for the FC's L4 RACE_TEST self-test (display segment test,
// sensor beam-break check, SC comm check). Owned by handleRaceTest();
// reset() on RACE_TEST entry. failCodes collect 2xx codes that phase 4
// cycles on the displays as 00.XXX -- meanings in docs/race-test-codes.md.
// One instance: `fct`.
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
static FCTestCtx fct;
static unsigned long countdownEntryMs = 0;   // P1-11: SC-silent timeout anchor

static HeatResults    heatResult	= {};
static TimingInputs timingInputs	= {};

// State machine instance
static stateMachine stm			= {RACE_IDLE, RACE_IDLE, true, false};
static raceMode currentMode;

// State handlers (file-local) -- one per race state, dispatched from finishControllerLoop()
static void handleIdle();
static void handleStaging();
static void handleCountdown();
static void handleRacing();
static void handleComplete();
static void handleRaceTest();

// Internal helpers (file-local)
static void handleSensors();
static void handleRxReaction();
static void displayCarTimes();
static void displayReactionTimes();


// ==================== RACE_TEST SUPPORT ====================
// Display sequence: all-segments (88.888), then countdown 05->01 at 500ms each
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
	// Protocol on Serial1 (hardware UART, D0/D1); USB Serial stays free for
	// DERBY_DEBUG output so debug builds no longer corrupt the wire (P2-37).
	Serial1.begin(serialBaud);
	setupSerialBus(Serial1);
	setupSensors();
	setupDisplay();

	// Start in idle state.  These variables are declared in raceTypes.h.
    stm.current				= RACE_IDLE;
    stm.target				= RACE_IDLE;
    currentMode 			= MODE_GATEDROP;
}

void finishControllerLoop() {
    /*
     * Coordination model -- each transition has one initiator (calls selfTransition,
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

	switch(stm.current) {
		case RACE_IDLE:      handleIdle();      break;
		case RACE_STAGING:   handleStaging();   break;
		case RACE_COUNTDOWN: handleCountdown(); break;
		case RACE_RACING:    handleRacing();    break;
		case RACE_COMPLETE:  handleComplete();  break;
		case RACE_TEST:      handleRaceTest();  break;
		default:             stm.current = RACE_IDLE; break;
	}

	txService();

	// Symmetry with the SC: act on the verdict even though the FC has no
	// FATAL entries today, so a future FC message that IS fatal (Race
	// Manager era) needs only a table row, not new plumbing here.
	OutboxVerdict verdict = outbox.checkOutcomes();
	if (verdict.abortRequired) {
		DBG2("[FC] tx fail->forceIdle err=", (uint8_t)verdict.code);
		txError(verdict.code);
		stm.forceIdle();
	}
}

/* =========================================================================
 *                        RACE_IDLE HELPER FUNCTIONS
 * ========================================================================= */
// IDLE: between-heats rest state. Clears all heat data, blanks the displays,
// tracks mode changes from the SC, and follows the SC's ->STAGING.
static void handleIdle(){
	if (stm.takeEntry()) {
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
	stm.service();
}

/* =========================================================================
 *                        RACE_STAGING HELPER FUNCTIONS
 * ========================================================================= */
// STAGING: passive on the FC -- just follows the SC to COUNTDOWN or back
// to IDLE.
static void handleStaging(){
	if (stm.takeEntry()) {
		DBG("[FC] ->STAGING");
	}
	stm.service();
}

/* =========================================================================
 *                        RACE_COUNTDOWN HELPER FUNCTIONS
 * ========================================================================= */
// COUNTDOWN: waits for MSG_RACE_START -- stamps the race clock (t0) and arms
// the sensors the moment it arrives. Aborts to IDLE if the SC goes silent
// for 10 s. Follows the SC's ->RACING.
static void handleCountdown(){
	if (stm.takeEntry()) {
		DBG("[FC] ->COUNTDOWN");
		timingInputs.startUs = 0;
		countdownEntryMs     = millis();
	}

	// SC went silent -- sensors never armed, heat is unrecoverable. Abort to IDLE.
	if ((millis() - countdownEntryMs) > 10000UL) {
		DBG("[FC] countdown timeout->forceIdle");
		stm.forceIdle();
		return;
	}

	if (rx.RaceStart && (timingInputs.startUs == 0)) {
		timingInputs.startUs = micros();
		armSensors(timingInputs.startUs);
	}
	stm.service();
}

/* =========================================================================
 *                        RACE_RACING HELPER FUNCTIONS
 * ========================================================================= */
// RACING: collects sensor finishes (or DNFs at maxRaceTimeUs) and inbound
// reaction/foul messages. Initiates ->COMPLETE once both lanes are recorded;
// disarms sensors on exit.
static void handleRacing(){
	if (stm.takeEntry()) {
		DBG("[FC] ->RACING");
		timingInputs.leftRecorded		= false;
		timingInputs.rightRecorded		= false;
		timingInputs.leftTimeUs			= 0;
		timingInputs.rightTimeUs		= 0;
		timingInputs.leftDnf			= false;
		timingInputs.rightDnf			= false;
		// Only arm if not already armed from COUNTDOWN state
		if (timingInputs.startUs == 0) {
			timingInputs.startUs		= micros();
			armSensors(timingInputs.startUs);
		}
	}

	handleSensors();
	handleRxReaction();

	if (timingInputs.leftRecorded && timingInputs.rightRecorded) {
		stm.request(RACE_COMPLETE);
	}
	// The FC initiates the exit from RACING; received state edges are held
	// (an SC abort arrives as MSG_ERROR and forceIdles at loop level).
	stm.service(true);

	if (stm.takeExit()) {
		disarmSensors();
	}
}

/* =========================================================================
 *                        RACE_COMPLETE HELPER FUNCTIONS
 * ========================================================================= */
// COMPLETE: computes heat results once at entry, shows car times, sends
// MSG_WINNER, then steps through display advances from the SC (car times ->
// reaction times -> done). Initiates ->IDLE after the last advance.
static void handleComplete(){
	if (stm.takeEntry()) {
		DBG("[FC] ->COMPLETE");
		handleRxReaction();   // fold in reaction/foul messages still in flight at the finish
		computeHeatResults(heatResult, timingInputs);

		// Warn if reaction times were expected but never received (lost serial TX).
		// Car times will equal race times -- incorrect but best-effort; result is still displayed.
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
		uint8_t winnerMask = 0;
		if (!leftValid && !rightValid) {
			winnerMask = winner_noResult;
		} else {
			if (heatResult.left.winner)  winnerMask |= winner_leftWin;
			if (heatResult.right.winner) winnerMask |= winner_rightWin;
			if (!heatResult.left.winner && !heatResult.right.winner) winnerMask |= winner_tie;
		}
		DBG2("[FC] winner mask=", winnerMask);
		// The engine resends from the payload captured here on failure, so the
		// mask no longer needs to outlive this pass.
		if (txWinner(winnerMask)) outbox.track(MSG_WINNER);
	}

	if(rx.DisplayAdvanceFlag) {
		if(needReact){
			displayReactionTimes();
			needReact			= false;
		} else if (!outbox.anyPending()) {
			// Advance to IDLE only once MSG_WINNER has resolved -- the gate
			// decides WHETHER to request, not whether to service.
			stm.request(RACE_IDLE);
		}
		rx.DisplayAdvanceFlag	= false;
	}
	// The FC initiates the exit from COMPLETE; received state edges are held
	// (an SC abort arrives as MSG_ERROR and forceIdles at loop level).
	stm.service(true);

	if (stm.takeExit()) {
		heatResult			= {};
		timingInputs		= {};
	}
}

/* =========================================================================
 *                        RACE_TEST HELPER FUNCTIONS
 * ========================================================================= */
// RACE_TEST: L4 self-test (entered when the SC sends RACE_TEST at boot).
// Phases: 1 display segment test, 2 sensor beam-break check, 3 SC comm
// check, 4 result display (permanent; power-cycle to exit). Codes:
// docs/race-test-codes.md.
static void handleRaceTest(){
	if (stm.takeEntry()) {
		DBG("[FC] ->RACE_TEST");
		fct.reset();
		fct.scCommReceived = ((raceState)rx.State == RACE_TEST);
		fct.phase = 1;
		fct.startTimer();
	}

			switch (fct.phase) {
				case 1: {  // Display test: 88.888 all-segments, then countdown 05->01
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

				case 4: {  // Result display -- stays here permanently (power-cycle to exit)
					if (fct.failCount == 0) {
						// All clear: 00.000 on both lanes
						if (fct.subPhase == 0) {
							updateDisplay(0UL, LANE_LEFT);
							updateDisplay(0UL, LANE_RIGHT);
							fct.subPhase = 1;
						}
						break;
					}

					// Failure cycle: show each code (2s) -> blank (1s) -> 88.888 end marker (1s) -> repeat
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

}

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
		timingInputs.leftReactionUs = (uint32_t)rx.LeftReactionTime;
		rx.LeftReactionValid        = false;
		rx.LeftReactionTime         = 0;
		DBG2("[FC] left react us=", timingInputs.leftReactionUs);
	}
	if (rx.RightReactionValid) {
		timingInputs.rightReactionUs = (uint32_t)rx.RightReactionTime;
		rx.RightReactionValid        = false;
		rx.RightReactionTime         = 0;
		DBG2("[FC] right react us=", timingInputs.rightReactionUs);
	}
	if (rx.LeftFoul) {
		timingInputs.leftFoul = true;
		rx.LeftFoul           = false;
		DBG("[FC] rx foul: LEFT");
	}
	if (rx.RightFoul) {
		timingInputs.rightFoul = true;
		rx.RightFoul           = false;
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
