/*
 * Pinewood Derby Track Start Controller
 * Version: 1.0
 * Author: Darren Schaaf
 * Compile: arduino-cli or PlatformIO (see README "Deployment")
 * Board: Arduino Nano AVR (ATmega328P)
 *
 * Owns the operator-facing half of the track: buttons, christmas-tree
 * lights, electromagnet gates, and reaction-time capture. Initiates the
 * IDLE->STAGING->COUNTDOWN->RACING transitions; follows the FC for
 * RACING->COMPLETE->IDLE. Structure: startControllerLoop() dispatches to
 * one handleXxx() function per race state (defined in the section blocks
 * below); cross-state helpers and the TX bookkeeping live at the bottom.
 */

#include <Arduino.h>
#include "startController.h"
#include "serialComm.h"
#include "outbox.h"
#include "stateMachine.h"
#include "lights.h"
#include "gates.h"
#include "buttons.h"
#include "debug.h"

static byte modeIndicatorPattern(raceMode mode) {
	switch (mode) {
		case MODE_GATEDROP: return LIGHT_Y1;
		case MODE_REACTION: return LIGHT_Y2;
		case MODE_PRO:      return LIGHT_Y3;
		default:            return LIGHT_GO;
	}
}

// Race-mode state machine (GATEDROP/REACTION/PRO/DIALIIN). Mirrors the
// stateMachine coordination pattern: the SC initiates mode changes from the
// MODE button (selfTransition, waits for FC ACK) and follows unsolicited
// changes from the FC/Race Manager (rxTransition). Only active in RACE_IDLE
// via handleModeChanges(). One instance: `md`.
struct modeSelect {
	raceMode current;
	raceMode target;
	raceMode nextMode() const {
		// Next mode in sequence for a button press
		switch(current) {
			case MODE_GATEDROP:	return MODE_REACTION;
			case MODE_REACTION:	return MODE_PRO;
			case MODE_PRO:		return MODE_GATEDROP;	// DIALIIN is skipped -- only Race Manager can enter it via BLE
			case MODE_DIALIIN:	return MODE_GATEDROP;	// exit path: operator can press mode to leave DIALIIN
			default:			return MODE_GATEDROP;
		}
	}

// Internal mechanics -- reachable only through service(), mirroring the
// shared stateMachine (see docs/adr/ADR-0006).
private:
	void selfTransition(raceMode newMode) {
		// 1. Check if already in target state
        if (current == newMode) {
            return;
        }

		// 2. Set intention to transition
		target = newMode;

		// 3. Enqueue coordinated change (no-op if already in flight)
		txRaceMode((uint8_t)target);

		// 4. Check current TX status
		switch (txStatusOf(MSG_RACE_MODE)) {

			case TX_ACKED:
				current = target;
				rx.Mode = (uint8_t)current;
				startBlink(modeIndicatorPattern(target), 0x00, 3, 250, LIGHT_OFF);
				return;

			case TX_TIMEOUT:
			case TX_FAILED:
				target = current;
				return;

			default:
				// TX_NONE, TX_SENT, TX_NACKED -- still waiting
				return;
		}
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

		// 3. Execute light pattern for new mode
		startBlink(modeIndicatorPattern(target), 0x00, 3, 250, LIGHT_OFF);

		return;

	}

public:
	// Declare-intent interface, mirroring stateMachine: request() records
	// where the mode should go; service() consumes an unsolicited mode from
	// the peer, then drives a pending request through send/ACK/commit.
	void request(raceMode next) {
		if (next == current) return;
		target = next;
	}

	void service() {
		if ((raceMode)rx.Mode != current) rxTransition((raceMode)rx.Mode);
		if (target != current) selfTransition(target);
	}
};


// Workbook for the SC's L4 RACE_TEST self-test (FC comm ping, light chase,
// gate cycle, interactive button check). Owned by handleRaceTest(); reset()
// on RACE_TEST entry. failCodes collect 1xx codes that phase 4 blinks on the
// red lights -- code meanings are listed in docs/race-test-codes.md.
// One instance: `rt`.
struct RaceTestCtx {
	uint8_t  phase;
	uint8_t  subPhase;
	unsigned long timer;
	uint8_t  failCodes[7];
	uint8_t  failCount;
	bool     btnArmed;

	void reset() {
		phase = 0; subPhase = 0; timer = 0;
		failCount = 0; btnArmed = false;
		memset(failCodes, 0, sizeof(failCodes));
	}
	void addFail(uint8_t c) { if (failCount < 7) failCodes[failCount++] = c; }
	bool elapsed(unsigned long ms) const {
		return (unsigned long)(millis() - timer) >= ms;
	}
	void startTimer() { timer = millis(); }
	void nextPhase(uint8_t p) { phase = p; subPhase = 0; startTimer(); }
};

// The SC's heat-event delivery rules (its rows of the ADR-0004 abort
// criteria): a permanently failed RACE_START means the FC's sensors were
// never armed, a lost FOUL means the FC would compute wrong car times --
// both make the heat untrustworthy. Reaction times and display advance are
// record-keeping only. queueHeatMsg() builds each payload and starts
// tracking; the main loop acts on checkOutcomes() verdicts (txError +
// forceIdle). RACING refuses to follow COMPLETE while outbox.anyPending()
// so heat data cannot be lost in transit.
static const OutboxEntry kOutboxTable[] = {
	{ MSG_RACE_START,   OutboxPolicy::FATAL,     err_START_TX_TIMEOUT },
	{ MSG_FOUL,         OutboxPolicy::FATAL,     err_STATE_TX_TIMEOUT },
	{ MSG_LEFT_REACT,   OutboxPolicy::TOLERATED, err_NULL },
	{ MSG_RIGHT_REACT,  OutboxPolicy::TOLERATED, err_NULL },
	{ MSG_DISP_ADVANCE, OutboxPolicy::TOLERATED, err_NULL },
};
static Outbox outbox = { kOutboxTable, sizeof(kOutboxTable) / sizeof(kOutboxTable[0]) };

// State & mode machine instances
static stateMachine stm					= {RACE_IDLE, RACE_IDLE, true, false};
static modeSelect md					= {MODE_GATEDROP, MODE_GATEDROP};

// timing
static raceTimingData raceTiming			= {0, {0, 0}};
static uint32_t tNow					= 0;

// countdown
// Drives the christmas-tree countdown timing: tick() advances
// CD_STAGED -> Y3 -> Y2 -> Y1 -> CD_GO on millis() intervals (400 ms in PRO
// mode, which skips straight to Y1; 500 ms otherwise). handleCountdown()
// watches changed() to fire light updates and the GO actions exactly once.
// Reset to CD_IDLE at IDLE entry, to CD_STAGED at COUNTDOWN entry.
// One instance: `cd`.
struct CountDownCtx {
	countdownState state  = CD_IDLE;
	countdownState prev   = CD_IDLE;
	unsigned long  timer  = 0;
	unsigned long  delay  = 0;

	bool changed() const { return state != prev; }

	void tick(raceMode mode) {
		prev = state;
		unsigned long now = millis();
		switch (state) {
			case CD_STAGED:
				delay = (mode == MODE_PRO) ? 400 : 500;
				state = (mode == MODE_PRO) ? CD_Y1 : CD_Y3;
				timer = now;
				break;
			case CD_Y3:
				if (now - timer >= delay) { state = CD_Y2; timer = now; }
				break;
			case CD_Y2:
				if (now - timer >= delay) { state = CD_Y1; timer = now; }
				break;
			case CD_Y1:
				if (now - timer >= delay) { state = CD_GO; timer = now; }
				break;
			default: break;
		}
	}
};
static CountDownCtx cd;

// results
static bool          winLightsPend  = false;   // true until winner data received or timed out
static unsigned long winLightTimer  = 0;        // millis() timestamp when RACE_COMPLETE was entered

// button management
static bool startReleased				= true;
static bool modeReleased				= true;
static RaceTestCtx rt;

// State handlers (file-local) -- one per race state, dispatched from startControllerLoop()
static void handleIdle();
static void handleStaging();
static void handleCountdown();
static void handleRacing();
static void handleComplete();
static void handleRaceTest();

// Internal helpers (file-local)
static void handleModeChanges();
static void handleEarlyStarts(unsigned long tn, raceMode mode);
static void handleCountdownGoActions(uint32_t tn);
static void handleTrackTriggers(uint32_t tn);
static void handleDisplayAdvance();
static void queueHeatMsg(serialMsgID id);

// ==================== RACE_TEST SUPPORT ====================
// Light chase sequence: L→R sweep, R→L sweep, 3× all-on/all-off
struct LightStep { byte pattern; uint16_t ms; };
static const LightStep kLightSteps[] = {
	{ LIGHT_BL, 150 }, { LIGHT_BR, 150 }, { LIGHT_Y1, 150 }, { LIGHT_Y2, 150 },
	{ LIGHT_Y3, 150 }, { LIGHT_GO, 150 }, { LIGHT_FL, 150 }, { LIGHT_FR, 150 },
	{ LIGHT_FR, 150 }, { LIGHT_FL, 150 }, { LIGHT_GO, 150 }, { LIGHT_Y3, 150 },
	{ LIGHT_Y2, 150 }, { LIGHT_Y1, 150 }, { LIGHT_BR, 150 }, { LIGHT_BL, 150 },
	{ 0xFF,     500 }, { 0x00,     500 }, { 0xFF,     500 },
	{ 0x00,     500 }, { 0xFF,     500 }, { 0x00,     500 },
};
static const uint8_t kLightStepCount = sizeof(kLightSteps) / sizeof(kLightSteps[0]);

// Button test sequence: fail code + confirm light + check function
struct BtnTest { uint8_t failCode; byte confirmLight; bool (*check)(); };
static const BtnTest kBtnTests[] = {
	{ 103, LIGHT_BL, isStartPressed },
	{ 104, LIGHT_BR, isModePressed  },
	{ 105, LIGHT_Y1, isLeftPressed  },
	{ 106, LIGHT_Y2, isRightPressed },
};
static const uint8_t kBtnTestCount = sizeof(kBtnTests) / sizeof(kBtnTests[0]);

void startControllerSetup(){
	setupSerial();
	setupButtons();
	setupGates();
	setupLights();

	// Start in idle state.  These variables are declared in raceTypes.h.
	stm.current					= RACE_IDLE;
	stm.target					= RACE_IDLE;
	md.current 				= MODE_GATEDROP;
	md.target 					= MODE_GATEDROP;

	// Hold MODE button at power-up to enter self-test mode
	if (isModePressed()) {
		stm.current = RACE_TEST;
		stm.target  = RACE_TEST;
		stm.entry   = true;
	}
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

	OutboxVerdict verdict = outbox.checkOutcomes();
	if (verdict.abortRequired) {
		// A FATAL heat message permanently failed -- the result cannot be
		// trusted. Tell the FC why, then reset the heat.
		DBG2("[SC] tx fail->forceIdle err=", (uint8_t)verdict.code);
		txError(verdict.code);
		stm.forceIdle();
	}
}

/* =========================================================================
 *                        RACE_IDLE HELPER FUNCTIONS
 * ========================================================================= */
// IDLE: between-heats rest state. Clears all heat data, allows mode changes,
// and starts a new heat on the Start button (initiates IDLE->STAGING).
static void handleIdle(){
	if (stm.takeEntry()) {
		DBG("[SC] ->IDLE");
		cd.state 		= CD_IDLE;
		raceTiming.reset();
		rx.clearHeatEvents();
		cancelBlink();
		updateLights(LIGHT_OFF);
		dropGate(LANE_LEFT);
		dropGate(LANE_RIGHT);
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

	if (!isBlinking()){
		if (isStartPressed())	stm.request(RACE_STAGING);		// Start moves to STAGING
	}

	stm.service();
}

 static void handleModeChanges(){
 	// Handle mode changes via button press or rxSerial.
	// Mode changes preempt any in-progress blink -- startBlink() in selfTransition/rxTransition overwrites.
	// Unsolicited peer changes (rx.Mode) take precedence over the button;
	// md.service() consumes them.
	if ((raceMode)rx.Mode == md.current){
		if (!isModePressed())		modeReleased	= true;		// button released, ready for next detection
		if (isModePressed() && modeReleased){
			modeReleased			= false;					// don't revisit until released
			md.request(md.nextMode());							// advance to the next operator-selectable mode
		}
	}
	md.service();
}

 /* =========================================================================
 *                        RACE_STAGING HELPER FUNCTIONS
 * ========================================================================= */
// STAGING: gates return so cars can be loaded. Start (with gates ready)
// initiates ->COUNTDOWN; Mode initiates ->IDLE; follows FC aborts.
static void handleStaging(){
	if (stm.takeEntry()) {
		DBG("[SC] ->STAGING");
		returnGates(); 											// reset the gate status to park the cars
		cancelBlink();												// stop any pending blink before setting steady state
		updateLights(LIGHT_BL | LIGHT_BR); 						// set the lights to blue
	}

	updateBlink();

	updateGates();

	if (!isBlinking()){
		if (isStartPressed() && areLanesReady())	stm.request(RACE_COUNTDOWN);	// Start moves to COUNTDOWN
		if (isModePressed())	stm.request(RACE_IDLE);			// Mode returns to IDLE
	}

	stm.service();			// also follows FC-initiated abort to IDLE
}

/* =========================================================================
 *                        RACE_COUNTDOWN HELPER FUNCTIONS
 * ========================================================================= */
// COUNTDOWN: runs the tree via cd.tick(), records early starts as fouls,
// and at GO captures the race-start timestamp, queues MSG_RACE_START, and
// initiates ->RACING. Follows FC aborts.
static void handleCountdown(){
	if (stm.takeEntry()) {
		DBG("[SC] ->COUNTDOWN");
		cd.state  = CD_STAGED;
	}

	tNow = micros();

	handleEarlyStarts(tNow, md.current);

	cd.tick(md.current);

	if (cd.state == CD_GO && cd.changed()) {
		handleCountdownGoActions(tNow);
	}
	if (cd.changed()) {
		byte cdLights = buildLightConfig(cd.state, raceTiming.isFoul(LANE_LEFT), raceTiming.isFoul(LANE_RIGHT), md.current);
		updateLights(cdLights);
	}

	stm.service();			// also follows FC-initiated abort to IDLE
}

static void handleEarlyStarts(unsigned long tn, raceMode mode){
	if (mode != MODE_GATEDROP){
		if (isLeftPressed() && isLaneUp(LANE_LEFT)){
			DBG("[SC] early start: LEFT");
			raceTiming.recordTrigger(LANE_LEFT,  tn);
			dropGate(LANE_LEFT);
		}
		if (isRightPressed() && isLaneUp(LANE_RIGHT)){
			DBG("[SC] early start: RIGHT");
			raceTiming.recordTrigger(LANE_RIGHT, tn);
			dropGate(LANE_RIGHT);
		}
	}
}

static void handleCountdownGoActions(uint32_t tn){
	DBG("[SC] GO -> queue RACE_START");
	stm.request(RACE_RACING);
	raceTiming.recordRaceStart(tn);
	queueHeatMsg(MSG_RACE_START);

	if (md.current == MODE_GATEDROP){
		dropGate(LANE_LEFT);
		dropGate(LANE_RIGHT);
	}
}


/* =========================================================================
 *                        RACE_RACING HELPER FUNCTIONS
 * ========================================================================= */
// RACING: sends foul status (and fouled lanes' reaction times) at entry,
// captures lane releases in REACTION/PRO mode, then follows the FC's
// ->COMPLETE once all queued messages have resolved.
static void handleRacing(){
	if (stm.takeEntry()) {
		DBG("[SC] ->RACING");
		queueHeatMsg(MSG_FOUL);
		// Fouled lanes triggered during COUNTDOWN -- their gates are already
		// dropped so handleTrackTriggers() can never fire for them. Send their
		// reaction times now so FC's foul car-time math has real data.
		if (raceTiming.isFoul(LANE_LEFT))  queueHeatMsg(MSG_LEFT_REACT);
		if (raceTiming.isFoul(LANE_RIGHT)) queueHeatMsg(MSG_RIGHT_REACT);
	}

	tNow = micros();

	if (md.current != MODE_GATEDROP){
		handleTrackTriggers(tNow);
	}

	// Data-integrity hold: only follow the FC's COMPLETE once every queued
	// heat message has resolved. The hold defers the state message; it is
	// applied on the first pass after the queue drains.
	stm.service(outbox.anyPending());
}

static void handleTrackTriggers(uint32_t tn){
	if (isLeftPressed() && isLaneUp(LANE_LEFT)){
		DBG("[SC] trigger: LEFT");
		raceTiming.recordTrigger(LANE_LEFT,  tn);
		dropGate(LANE_LEFT);
		queueHeatMsg(MSG_LEFT_REACT);
	}
	if (isRightPressed() && isLaneUp(LANE_RIGHT)){
		DBG("[SC] trigger: RIGHT");
		raceTiming.recordTrigger(LANE_RIGHT, tn);
		dropGate(LANE_RIGHT);
		queueHeatMsg(MSG_RIGHT_REACT);
	}
}


 /* =========================================================================
 *                        RACE_COMPLETE HELPER FUNCTIONS
 * ========================================================================= */
// COMPLETE: waits for MSG_WINNER (2 s timeout), blinks the win lights, and
// forwards Start presses as display advances. Follows the FC's ->IDLE once
// the win-light animation finishes.
static void handleComplete(){
	if (stm.takeEntry()) {
		DBG("[SC] ->COMPLETE");
		winLightsPend  = true;
		winLightTimer  = millis();
		cancelBlink();
	}

	handleDisplayAdvance();

	if (winLightsPend) {
		// Hold until MSG_WINNER arrives from FC, or fall through after ~2s if it never does.
		bool timedOut = ((unsigned long)(millis() - winLightTimer) >= 2000UL);
		if (rx.WinnerReceived || timedOut) {
			if (rx.LeftWin)  startBlink(LIGHT_GO | LIGHT_FR, LIGHT_FR, 3, 250, LIGHT_GO | LIGHT_FR);
			if (rx.RightWin) startBlink(LIGHT_GO | LIGHT_FL, LIGHT_FL, 3, 250, LIGHT_GO | LIGHT_FL);
			if (rx.Tie)      startBlink(LIGHT_GO, 0x00, 3, 250, LIGHT_GO);
			// rx.NoResult (double-foul): no win lights, just clear pend
			winLightsPend = false;
		}
	}

	// Follow the FC to IDLE only after the win-light animation finishes --
	// updateBlink() both drives the animation and reports it still running.
	if (!winLightsPend && !updateBlink()) {
		stm.service();
	}
}

static void handleDisplayAdvance(){
	if (isStartPressed() && startReleased){
		startReleased		= false;
		queueHeatMsg(MSG_DISP_ADVANCE);
	}
	if (!isStartPressed()) startReleased = true;
}

 /* =========================================================================
 *                        RACE_TEST HELPER FUNCTIONS
 * ========================================================================= */

// RACE_TEST: L4 self-test (entered by holding MODE at power-up). Phases:
// 0 FC comm ping, 1 light chase, 2 gate cycle, 3 button prompts, 4 result
// display (permanent; power-cycle to exit). Codes: docs/race-test-codes.md.
static void handleRaceTest(){
	if (stm.takeEntry()) {
		DBG("[SC] ->RACE_TEST");
		cancelBlink();
		updateLights(LIGHT_OFF);
		rt.reset();
	}

	switch (rt.phase) {
		case 0: {  // FC comm ping: verify FC responds to RACE_TEST state message
			switch (rt.subPhase) {
				case 0:
					txRaceState((uint8_t)RACE_TEST);
					rt.startTimer();
					rt.subPhase = 1;
					break;
				case 1: {
					txStatus s = txStatusOf(MSG_RACE_STATE);
					if (s == TX_ACKED) {
						startBlink(LIGHT_BL | LIGHT_BR, LIGHT_OFF, 3, 250, LIGHT_OFF);
						rt.nextPhase(1);
					} else if (s == TX_TIMEOUT || s == TX_FAILED || rt.elapsed(2000)) {
						rt.addFail(107);
						startBlink(LIGHT_FL | LIGHT_FR, LIGHT_OFF, 1, 250, LIGHT_OFF);
						rt.nextPhase(1);
					}
					break;
				}
			}
			break;
		}

		case 1: {  // Light chase: wait for Phase 0 feedback blink, then step through kLightSteps[]
			updateBlink();
			if (isBlinking()) break;
			if (rt.subPhase == 0) {
				updateLights(kLightSteps[0].pattern);
				rt.startTimer();
				rt.subPhase = 1;
			} else if (rt.elapsed(kLightSteps[rt.subPhase - 1].ms)) {
				if (rt.subPhase < kLightStepCount) {
					updateLights(kLightSteps[rt.subPhase].pattern);
					rt.startTimer();
					rt.subPhase++;
				} else {
					updateLights(LIGHT_OFF);
					rt.nextPhase(2);
				}
			}
			break;
		}

		case 2: {  // Gates: return both, verify up, drop L then R
			switch (rt.subPhase) {
				case 0:
					returnGates();
					rt.startTimer();
					rt.subPhase = 1;
					break;
				case 1:
					updateGates();
					if (rt.elapsed(600)) {
						if (!areLanesReady()) {
							if (!isLaneUp(LANE_LEFT))  rt.addFail(101);
							if (!isLaneUp(LANE_RIGHT)) rt.addFail(102);
						}
						dropGate(LANE_LEFT);
						rt.startTimer();
						rt.subPhase = 2;
					}
					break;
				case 2:
					if (rt.elapsed(300)) {
						dropGate(LANE_RIGHT);
						rt.startTimer();
						rt.subPhase = 3;
					}
					break;
				case 3:
					if (rt.elapsed(300)) rt.nextPhase(3);
					break;
			}
			break;
		}

		case 3: {  // Buttons: prompt each of 4 buttons in sequence, 5s timeout per button
			if (rt.subPhase >= kBtnTestCount) {
				updateBlink();
				if (!isBlinking()) {
					updateLights(LIGHT_OFF);
					rt.nextPhase(4);
				}
				break;
			}

			updateBlink();
			bool pressed = kBtnTests[rt.subPhase].check();

			if (!rt.btnArmed) {
				// Wait for the button to be released before starting the 5s countdown
				if (!pressed) {
					rt.btnArmed = true;
					rt.startTimer();
				}
				break;
			}

			// GO blink = "press a button" prompt; restarts each short cycle
			if (!isBlinking()) startBlink(LIGHT_GO, LIGHT_OFF, 2, 400, LIGHT_GO);

			if (pressed) {
				cancelBlink();
				startBlink(kBtnTests[rt.subPhase].confirmLight, LIGHT_OFF, 3, 150, LIGHT_OFF);
				rt.subPhase++;
				rt.btnArmed = false;
			} else if (rt.elapsed(5000)) {
				rt.addFail(kBtnTests[rt.subPhase].failCode);
				rt.subPhase++;
				rt.btnArmed = false;
			}
			break;
		}

		case 4: {  // Result display — stays here permanently (power-cycle to exit)
			updateBlink();
			if (rt.failCount == 0) {
				// All pass: GO blinks 5x then stays on
				if (rt.subPhase == 0) {
					startBlink(LIGHT_GO, LIGHT_OFF, 5, 300, LIGHT_GO);
					rt.subPhase = 1;
				}
			} else {
				// Any failure: red blinks failCount times, pauses 1s, repeats
				if (rt.subPhase == 0 && rt.elapsed(1000)) {
					startBlink(LIGHT_FL | LIGHT_FR, LIGHT_OFF, rt.failCount, 400, LIGHT_OFF);
					rt.subPhase = 1;
				} else if (rt.subPhase == 1 && !isBlinking()) {
					rt.startTimer();
					rt.subPhase = 0;
				}
			}
			break;
		}
	}

}

 /* =========================================================================
 *                        GENERIC HELPER FUNCTIONS
 * ========================================================================= */

// Builds the payload for a heat-event message, enqueues it, and starts
// outbox tracking. Tracking begins only if the enqueue was accepted -- a
// rejected duplicate is already tracked from its original send.
static void queueHeatMsg(serialMsgID id) {
	bool sent = false;
	switch(id) {
		case MSG_RACE_START:
			sent = txRaceStart();
			break;
		case MSG_FOUL: {
			uint8_t mask = (raceTiming.isFoul(LANE_LEFT)  ? foul_left  : 0)
			             | (raceTiming.isFoul(LANE_RIGHT) ? foul_right : 0);
			sent = txFoulStatus(mask);
			break;
		}
		case MSG_LEFT_REACT:
			sent = txReactionTime(raceTiming.reactionTimeUs(LANE_LEFT),  LANE_LEFT);
			break;
		case MSG_RIGHT_REACT:
			sent = txReactionTime(raceTiming.reactionTimeUs(LANE_RIGHT), LANE_RIGHT);
			break;
		case MSG_DISP_ADVANCE:
			sent = txDisplayAdvance();
			break;
		default:
			break;
	}
	if (sent) outbox.track(id);
}