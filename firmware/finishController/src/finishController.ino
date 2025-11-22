#include "sensors.h"
#include "globals.h"
#include "serialComm.h"
//#include "btComm.h"
//#include "display.h"

bool needReact 		= false;		// Flag to indicate if reaction time is needed

// Results structure for a lane.  Times are stored in microseconds
struct raceResults {
    uint8_t		carID;			// 4‑byte UID
	bool		left;			// indicates track left(true) or right(false);
    bool		foul;			// whether a foul occurred (false start)
    bool		winner;			// true if this lane won the race
    uint32_t	carTimeUs;		// computed car time including or excluding reaction
    uint32_t	raceTimeUs;		// raw finish time from sensors
    uint32_t	reactionTimeUs;	// reaction time measured at start
};

struct StateMachine {
	raceState current;
	raceState target;
	bool entry;
	bool exit;
	// Returns true when transition is complete
    bool transition(raceState newState) {
		//**************** How might we control this to only transition to allowed states?*********************
		if (target != newState) {
			target = newState;  // Set intention
		}
        if (current != target) {
			// Try to coordinate transition
			txStatus result = txRaceState(target);
			switch (result) {
				case TX_ACKED:
					entry = true;    // Exit current state
                    current = target;   // Commit transition
                    exit = true;   // Enter new state
                    resetTxState(MSG_RACE_STATE);
                    return true;
                case TX_TIMEOUT:
                case TX_FAILED:
                    target = current;   // Abandon transition
                    resetTxState(MSG_RACE_STATE);
                    // Log error or flash lights
                    return false;
                default:
                    return false;  // Still pending
            }
        }
        return true;  // Already in target state
    }
    // Handle unsolicited state changes from rxSerial
    void rxTransition(raceState rxState) {
        if (rxState != current) {
			transition(rxState);
        }
    }	
};

struct RaceTimingData {
    uint32_t raceStartUs;
    uint32_t leftTimeUs;
    uint32_t rightTimeUs;
    bool leftRecorded;
    bool rightRecorded;
};

// Static instances for left and right lanes; lifetime extends over loops.
static raceResults leftResults	= {0, true, false, false, 0, 0, 0};
static raceResults rightResults	= {0, false, false, false, 0, 0, 0};
static StateMachine stm			= {RACE_IDLE, RACE_IDLE, true, false};
static RaceTimingData race		= {0, 0, 0, false, false};

void setup() {
	setupSerial();
	setupSensors();
	setupDisplay();
	//setupBT();
	
	// Start in idle state.  These variables are declared in globals.h.
    stm.current = RACE_IDLE;
    stm.target  = RACE_IDLE;
    currentMode  = MODE_GATEDROP;
    targetMode   = MODE_GATEDROP;
}

void loop() {
	rxSerial();
	switch(stm.current) {
		case RACE_IDLE:
			if(stm.entry){
				stm.entry = false;
				clearDisplay(true);				// clear display (left)
				clearDisplay(false);			// clear display (right))
			}
			if (targetMode != currentMode){
				handleModeTransition(targetMode);			// manage mode transition
				// tx mode change to BLE raceManager
				// what if rx race mode from BLE raceManager
			}
			// can initiate state change if raceManager requests test mode over BLE
			// stm.transition(stm.target);						// transitions state if updated target
			stm.rxTransition(rxState);						// transitions state if received target
			if(stm.exit){
				stm.exit = false;
				// tx state change to BLE raceManager
			}
			break;
			
		case RACE_STAGING:
			if(stm.entry){
				stm.entry = false;
			}
			handleCarID();						// manage handoff of car ID between startController and raceManager
			stm.rxTransition(rxState);						// transitions state if received target
			if(stm.exit){
				stm.exit = false;
			}
			break;
			
		case RACE_COUNTDOWN:
			if(stm.entry){
				stm.entry 	= false;
				rxRaceStart	= false;
			}
			if (rxRaceStart && (race.raceStartUs == 0)) {
				race.raceStartUs	= micros();
				armSensors(race.raceStartUs);
			}
			stm.rxTransition(rxState);						// transitions state if received target	
			if(stm.exit){
				stm.exit = false;		
			}
			break;
			
		case RACE_RACING:
			if(stm.entry){
				// Reset recording flags and times
				race.leftRecorded	= false;
				race.rightRecorded	= false;
				race.leftTimeUs			= 0;
				race.rightTimeUs		= 0;
				stm.entry = false;
				// Only arm if not already armed from COUNTDOWN state
				if (race.raceStartUs == 0){
					race.raceStartUs = micros();
					armSensors(race.raceStartUs);
				}
			}
			handleSensors();					// check for interrupt and record finish time
			handleRxResults();					// store results from rxSerial
			if (stm.target != stm.current){
				handleStateTransition(stm.target);			// manage state transition
			}
			
			if(stm.exit){
				stm.exit = false;	
				disarmSensors();	
			}
			break;
			
		case RACE_COMPLETION:
			if(stm.entry){
				needReact				= false;	// clear flag for safety
				rxDisplayAdvanceFlag	= false;	// clear flag for safety
				computeRaceTimes();					// calculate and compile race times, reaction times, and winner
				transmitWinnerToSC();				// send winner over serial to startController
				//transmitResultsToRM();			// send results over BT to raceManager
				displayCarTimes();					// push car times to display
				stm.entry 					= false;	// done with stm.entry tasks
			}
			if(rxDisplayAdvanceFlag) {
				if(needReact){
					displayReactionTimes();
				} else {
					stm.target			= RACE_IDLE;
				}
				rxDisplayAdvanceFlag	= false;
			}
			if (stm.target != stm.current){
				handleStateTransition(stm.target);			// manage state transition
			}
			if(stm.exit){
				stm.exit = false;
				// carID, left, foul, winner, carTimeUs, raceTimeUs, reactionTimeUs
				leftResults		= {0,	true,	false,	false,	0,	0,	0};		// reset left results struct
				rightResults	= {0,	false,	false,	false,	0,	0,	0};		// reset right results struct
			}
			break;
			
		case RACE_TEST:
			// currently unused, just transition back to idle
			if(stm.entry){
				stm.target = RACE_IDLE;
				stm.entry = false;
			}
			if (stm.target != stm.current){
				handleStateTransition(stm.target);			// manage state transition
			}
			if(stm.exit){
				stm.exit = false;
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
static void handleCarID() {
	// manage handoff of car ID between startController and raceManager
	if (rxLeftID != 0){
		// leftResults.carID = rxLeftID; 	// update onboard ID
		// rxLeftID = 0; 					// clear serial received ID
		// (post left ID to BLE) 			// send to raceManager
	}
	if (rxRightID !=0) {
		// rightResults.carID = rxRightID;	// update onboard ID
		// rxRightID = 0;					// clear serial received ID
		// (post right ID to BLE)			// send to raceManager
	}
	if (btLeftID !=0){
		// transmit over serial left ID 	// send to startController
		// btLeftID = 0; 					// clear BLE received ID
		// dont store the value, raceManager is only used for confirmation and not data source
		
	}
	if (btRightID !=0){
		// transmit over serial right ID 	// send to startController
		// btRightID = 0; 					// clear BLE received ID
		// dont store the value, raceManager is only used for confirmation and not data source
	}
}

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
	
	if (race.leftRecorded && race.rightRecorded) {
		stm.target			= RACE_COMPLETION;			// initiate state transition when both sensors recorded
	}
}

void handleRxResults() {
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
 *                        RACE_COMPLETION HELPER FUNCTIONS
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
	static bool txWin = true;
	while (txWin) {
		txStatus win = txWinner(winnerMask);
		switch (win) {
			case TX_ACKED:										
				txWin 		= false;						// winner transmission no longer pending
				resetTxState(MSG_WINNER);
				break;
			case TX_TIMEOUT:
			case TX_FAILED:
				txWin 		= false;						// winner transmission failed
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
}

void transmitResultsToRM(){
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
void handleModeTransition(raceMode target) {
	// coordinate mode transition to COMPLETION with raceManager over BT (protocol TBD)
	// coordinate mode transition to COMPLETION with startController over Serial
	
	// MSG_RACE_MODE  --> rxMode
		txStatus md = txRaceMode(target);
		if (md == TX_ACKED) {
			currentMode	= target;
	}
}