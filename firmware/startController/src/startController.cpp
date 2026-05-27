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
#include "startController.h"
#include "serialComm.h"
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

struct modeSelect {
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
};


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

struct PendingTx {
	bool raceStart  = false;
	bool foulStatus = false;
	bool leftReact  = false;
	bool rightReact = false;
	bool dispAdv    = false;

	bool anyPending() const {
		return raceStart || foulStatus || leftReact || rightReact || dispAdv;
	}
	void queue(serialMsgID id);       // enqueues the message and sets the flag
	void checkOutcomes();             // polls txStatusOf() and clears flags on terminal status
};

// State & mode machine instances
static stateMachine stm					= {RACE_IDLE, RACE_IDLE, true, false};
static modeSelect md					= {MODE_GATEDROP, MODE_GATEDROP};

// timing
static raceTimingData raceTime			= {0, {0, 0}};
static uint32_t tNow					= 0;

// countdown
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

// racing
static PendingTx pending;
// results
static bool          winLightsPend  = false;   // true until winner data received or timed out
static unsigned long winLightTimer  = 0;        // millis() timestamp when RACE_COMPLETE was entered

// error
static bool criticalTxError				= false;		// set on permanent failure of a critical message
static errCode pendingErrorCode			= err_NULL;		// error code to report to FC on criticalTxError

// button management
static bool startReleased				= true;
static bool modeReleased				= true;
static RaceTestCtx rt;

// Internal helpers (file-local)
static void handleModeChanges();
static void handleEarlyStarts(unsigned long tn, raceMode mode);
static void handleCountdownGoActions(long tn);
static void handleTrackTriggers(uint32_t tn);
static void handleDisplayAdvance();

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

	if (criticalTxError) {
		txError(pendingErrorCode);				// best-effort notify FC; idempotent after TX_FAILED/TX_TIMEOUT
		updateLights(LIGHT_FL | LIGHT_FR);		// both red = critical failure, power-cycle required
		return;
	}

	switch(stm.current) {
		case RACE_IDLE:
			if(stm.entry){
				stm.entry		= false;
				DBG("[SC] ->IDLE");
				cd.state 		= CD_IDLE;
				raceTime.reset();
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
				DBG("[SC] ->STAGING");
				returnGates(); 											// reset the gate status to park the cars
				cancelBlink();												// stop any pending blink before setting steady state
				updateLights(LIGHT_BL | LIGHT_BR); 						// set the lights to blue
			}

			updateBlink();

			updateGates();

			if (!isBlinking()){
				if (isStartPressed() && areLanesReady())	stm.target = RACE_COUNTDOWN;	// Start moves to COUNTDOWN
				if (isModePressed())	stm.target = RACE_IDLE;			// Mode returns to IDLE
			}

			if (stm.target != stm.current) stm.selfTransition(stm.target);

			if(stm.exit){
				stm.exit = false;
			}
			break;

		case RACE_COUNTDOWN:
			if(stm.entry){
				stm.entry = false;
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
				byte cdLights = buildLightConfig(cd.state, raceTime.isFoul(LANE_LEFT), raceTime.isFoul(LANE_RIGHT), md.current);
				updateLights(cdLights);
			}

			if (stm.target != stm.current) stm.selfTransition(stm.target);

			if(stm.exit){
				stm.exit = false;
			}
			break;
			
		case RACE_RACING:
			if(stm.entry){
				stm.entry  = false;
				DBG("[SC] ->RACING");
				pending.queue(MSG_FOUL);
			}

			tNow = micros();

			if (md.current != MODE_GATEDROP){
				handleTrackTriggers(tNow);
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
				stm.entry      = false;
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

			if (!winLightsPend && !updateBlink()) {   // updateBlink() drives the animation each loop
				stm.rxTransition((raceState)rx.State);
			}

			if(stm.exit){
				stm.exit = false;
			}
			break;
			
		case RACE_TEST:
			if (stm.entry) {
				stm.entry = false;
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

			if (stm.exit) {
				stm.exit = false;
			}
			break;

		default:
			stm.current = RACE_IDLE;
			break;
	}

	txService();
	pending.checkOutcomes();
}

/* =========================================================================
 *                        RACE_IDLE HELPER FUNCTIONS
 * ========================================================================= */
 static void handleModeChanges(){
 	// Handle mode changes via button press or rxSerial.
	// Mode changes preempt any in-progress blink — startBlink() in selfTransition/rxTransition overwrites.
	if (rx.Mode != md.current){
		md.rxTransition((raceMode)rx.Mode);								// Handle unsolicited mode changes from rxSerial
	} else {
		if (!isModePressed())		modeReleased	= true;		// button released, ready for next detection
		if (isModePressed() && modeReleased){
			modeReleased			= false;					// don't revisit until released
			md.nextMode();										// Select mode to advance to per transition order
		}
	}
	if (md.target != md.current) md.selfTransition(md.target);
}

 /* =========================================================================
 *                        RACE_STAGING HELPER FUNCTIONS
 * ========================================================================= */

/* =========================================================================
 *                        RACE_COUNTDOWN HELPER FUNCTIONS
 * ========================================================================= */
static void handleEarlyStarts(unsigned long tn, raceMode mode){
	if (mode != MODE_GATEDROP){
		if (isLeftPressed() && isLaneUp(LANE_LEFT)){
			DBG("[SC] early start: LEFT");
			raceTime.recordTrigger(LANE_LEFT,  tn);
			dropGate(LANE_LEFT);
		}
		if (isRightPressed() && isLaneUp(LANE_RIGHT)){
			DBG("[SC] early start: RIGHT");
			raceTime.recordTrigger(LANE_RIGHT, tn);
			dropGate(LANE_RIGHT);
		}
	}
}

static void handleCountdownGoActions(long tn){
	DBG("[SC] GO -> queue RACE_START");
	stm.target = RACE_RACING;
	raceTime.recordRaceStart(tn);
	pending.queue(MSG_RACE_START);

	if (md.current == MODE_GATEDROP){
		dropGate(LANE_LEFT);
		dropGate(LANE_RIGHT);
	}
}


/* =========================================================================
 *                        RACE_RACING HELPER FUNCTIONS
 * ========================================================================= */

static void handleTrackTriggers(uint32_t tn){
	if (isLeftPressed() && isLaneUp(LANE_LEFT)){
		DBG("[SC] trigger: LEFT");
		raceTime.recordTrigger(LANE_LEFT,  tn);
		dropGate(LANE_LEFT);
		pending.queue(MSG_LEFT_REACT);
	}
	if (isRightPressed() && isLaneUp(LANE_RIGHT)){
		DBG("[SC] trigger: RIGHT");
		raceTime.recordTrigger(LANE_RIGHT, tn);
		dropGate(LANE_RIGHT);
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

void PendingTx::queue(serialMsgID id) {
	switch(id) {
		case MSG_RACE_START:
			if (txRaceStart(0b0001))                                    raceStart  = true;
			break;
		case MSG_FOUL: {
			uint8_t mask = (raceTime.isFoul(LANE_LEFT)  ? foul_left  : 0)
			             | (raceTime.isFoul(LANE_RIGHT) ? foul_right : 0);
			if (txFoulStatus(mask)) foulStatus = true;
			break;
		}
		case MSG_LEFT_REACT:
			if (txReactionTime(raceTime.reactionTimeUs(LANE_LEFT),  LANE_LEFT))  leftReact  = true;
			break;
		case MSG_RIGHT_REACT:
			if (txReactionTime(raceTime.reactionTimeUs(LANE_RIGHT), LANE_RIGHT)) rightReact = true;
			break;
		case MSG_DISP_ADVANCE:
			if (txDisplayAdvance())                                     dispAdv    = true;
			break;
		default:
			break;
	}
}

void PendingTx::checkOutcomes() {
	if (raceStart) {
		txStatus s = txStatusOf(MSG_RACE_START);
		if (s == TX_ACKED) {
			raceStart = false;
		} else if (s == TX_TIMEOUT || s == TX_FAILED) {
			// FC sensors may not be armed — heat is unrecoverable. Abort to IDLE.
			raceStart = false;
			DBG("[SC] RACE_START fail->forceIdle");
			txError(err_START_TX_TIMEOUT);
			stm.forceIdle();
		}
	}
	if (foulStatus) {
		txStatus s = txStatusOf(MSG_FOUL);
		if (s == TX_ACKED || s == TX_TIMEOUT || s == TX_FAILED) {
			if (s != TX_ACKED) {
				// Foul data lost — result cannot be trusted. Abort to IDLE.
				DBG("[SC] FOUL fail->forceIdle");
				txError(err_STATE_TX_TIMEOUT);
				stm.forceIdle();
			}
			foulStatus = false;
		}
	}
	if (leftReact) {
		txStatus s = txStatusOf(MSG_LEFT_REACT);
		if (s == TX_ACKED || s == TX_TIMEOUT || s == TX_FAILED) {
			// Soft fail: reaction time is record-keeping only in REACTION/PRO mode.
			// FC already warned via P1-8 guard; continue without halting.
			leftReact = false;
		}
	}
	if (rightReact) {
		txStatus s = txStatusOf(MSG_RIGHT_REACT);
		if (s == TX_ACKED || s == TX_TIMEOUT || s == TX_FAILED) {
			rightReact = false;
		}
	}
	if (dispAdv) {
		txStatus s = txStatusOf(MSG_DISP_ADVANCE);
		if (s == TX_ACKED || s == TX_TIMEOUT || s == TX_FAILED) {
			dispAdv = false;
		}
	}
}