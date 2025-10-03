/*
 * Pinewood Derby Track Start Controller
 * Version: 1.0
 * Author: Darren Schaaf
 * Date September 2025
 * Compile: Arduino IDE 1.8+ or PlatformIO
 * Board: Arduino Nano
 * Libraries Required:
 *   - MFRC522 (for RFID)
 *   - SPI (built-in)
 */


#include "lights.h"
#include "gates.h"
#include "rfid.h"
#include "serialComm.h"
#include "buttons.h"
#include "globals.h"

// globals.h definitions
unsigned long leftReactionTime			= 0;			// Reaction time for left track
unsigned long rightReactionTime			= 0;			// Reaction time for right track
unsigned long raceStartTime				= 0;			// Log the start time of the race
unsigned long leftStartTime				= 0;			// Log the start time of the left track
unsigned long rightStartTime			= 0;			// Log the start time of the right track
bool leftFoul							= false;		// Log foul status of left track
bool rightFoul							= false;		// Log foul status of right track
raceState targetState					= RACE_IDLE;	// default target state
raceState currentState					= RACE_IDLE;	// default current state
raceMode targetMode						= MODE_GATEDROP;// default target mode
raceMode currentMode					= MODE_GATEDROP;// default current mode


static_assert(UID_LEN == serialUIDLength, "UID Lengths Must Match");
// state & mode transitions
static bool stateFirstPass				= true;			// marker for first pass of a state
static unsigned long statusLightTimer	= 0;			// timer used for tracking light status
static const unsigned long statusLightMax	= 3000;			// timer limit for displaying status
static bool modeChangeLights			= false;		// mode lights pending
static bool modeTxPending				= false;		// mode tx pending

// countdown 
static countdownState cdState			= CD_IDLE;		// current countdownState value - see globals.h
static countdownState prevCdState		= CD_IDLE;		// previous countdownState
static unsigned long cdTimer			= 0;			// countdown timer
static unsigned long stageDelay			= 500;			// default delay between staging sequences
static bool pendStartTx					= false;		// marker for if start transmission is pending

// racing
static bool reactLeftPending			= false;		// marker for if left reaction tx is pending
static bool reactRightPending			= false;		// marker for if right reaction tx is pending
static bool foulStatusPending			= false;		// marker for if foul status tx is pending
static unsigned long startDelay				= 0;			// Unused, delay between start and ACK

// rfid
RFIDResult rfidLStat;									// result from left reader
RFIDResult rfidRStat;									// result from right reader
static bool txLID						= false;		// marker for if left ID is tx pending
static bool txRID						= false;		// marker for if right ID is tx pending

// results
static bool winLightsPend				= false;		// marker if result lights need to display
static bool dispAdv						= false;		// marker for pending tx display advance

// button management
static bool startReleased				= true;
static bool modeReleased				= true;


// Helper to map mode -> lights
static inline uint8_t lightsForMode(raceMode m) {
  switch (m) {
    case MODE_GATEDROP: return LIGHT_Y1;
    case MODE_REACTION: return LIGHT_Y2;
    case MODE_PRO:      return LIGHT_Y3;
    case MODE_DIALIIN:  /* fall-through */
    default:            return LIGHT_GO;
  }
}

void setup(){
	setupSerial();
	setupButtons();
	setupGates();
	setupLights();
	setupRFID();														// note this returns a bool
}

void loop(){
	
	switch(currentState) {
		case RACE_IDLE:
			if(stateFirstPass){
					cdState = CD_IDLE;
					updateLights(LIGHT_OFF);
					dropGate(gateL);										// make sure gate L isn't up
					dropGate(gateR);										// make sure gate R isn't up
					targetState		= currentState;							// clear any target state collision
					targetMode		= currentMode;							// clear any target mode collision
					rxState			= currentState;							// clear any rxState collision
					rxMode			= currentMode;							// clear any rxMode collision
					modeReleased	= true;									// assume button not pressed
					startReleased	= true;									// assume button not pressed
					stateFirstPass 	= false;
				}
				
			if (rxSerial()){
				// Future: add other message-specific actions
			}
			
			if (isModePressed() && modeReleased){
				modeReleased			= false;							// don't revisit until released
				// Select mode to advance to per transition order
				switch (targetMode) {
					case MODE_GATEDROP: targetMode	= MODE_REACTION;	break;
					case MODE_REACTION: targetMode	= MODE_PRO;			break;
					case MODE_PRO:      targetMode	= MODE_DIALIIN;		break;
					default:            targetMode	= MODE_GATEDROP;	break;
				}
				resetTxState(MSG_RACE_MODE);								// reset TX status incase one is pending
			}
			if (!isModePressed())		modeReleased	= true; 			// button released, ready for next detection

			if (modeChangeLights){
				if(millis() - statusLightTimer > statusLightMax){
					updateLights(LIGHT_OFF);							// turn off mode status lights
					modeChangeLights 		= false; 					// status update complete
				}
			}
			
			if(currentMode != targetMode){ 
				// handle mode tx
				txStatus m = txRaceMode(targetMode); 					// helper function handles transmission status
				switch (m) {
					case TX_ACKED:										
						currentMode 		= targetMode;				// commit mode change
						rxMode				= currentMode;				// clear any rxMode collision
						modeChangeLights	= true;
						updateLights(lightsForMode(currentMode));		// set lights to show new mode
						statusLightTimer	= millis();
						resetTxState(MSG_RACE_MODE);
						break;
					case TX_TIMEOUT:
					case TX_FAILED:
						targetMode 			= currentMode;				// abandon mode change
						resetTxState(MSG_RACE_MODE);					// reset transmit message
						// future: flash red lights for error; updateLights(LIGHT_FL | LIGHT_FR);
						// future: log / transmit state transition error
						break;
					case TX_NONE:
					case TX_SENT:
					case TX_NACKED:
					default:
						break;
				}
			} else{
				if (currentMode != rxMode) {
					currentMode 		= rxMode;						// commit mode change
					targetMode 			= currentMode;					// clear any targetMode collision
					modeChangeLights	= true;							// mode lights need to be displayed
					updateLights(lightsForMode(currentMode));			// set lights to show new mode
					statusLightTimer	= millis();
					resetTxState(MSG_RACE_MODE);
				}
			}
		
			// When user presses the start button it should trigger a state transition
			if (currentState == targetState){
				// only check the button if we aren't already processing a state change
				if (isStartPressed()){
					targetState 			= RACE_STAGING;
				}
				if (rxState == RACE_TEST){
					currentState = rxState;								// allow transition to test remotely
					targetState = currentState;							// clear any pending target state transition
				}
			} else {
				txStatus s = txRaceState(targetState); 					// helper function handles transmission status
				switch (s) {
					case TX_ACKED:										
						currentState 		= targetState;				// commit state change
						resetTxState(MSG_RACE_STATE);
						stateFirstPass 		= true;						// reset first pass marker
						break;
					case TX_TIMEOUT:
					case TX_FAILED:
						targetState			= currentState;				// abandon state transition
						resetTxState(MSG_RACE_STATE);					// reset transmit message
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
			break;
			
		case RACE_STAGING:
			if(stateFirstPass){
				returnGates(); 										// reset the gate status to park the cars
				updateLights(LIGHT_BL | LIGHT_BR); 					// set the lights to blue
				memset(leftCarID, 0, UID_LEN);						// clear rx for left ID
				memset(rightCarID, 0, UID_LEN);						// clear rx for right ID
				targetState			= currentState;					// clear any target state collision
				rxState				= currentState;					// clear any rxState collision
				blinkState.active 	= false;  						// Clear any pending blinks
				stateFirstPass 		= false;
			}
			if(gateStatus.returnActive)	returnGates();				// call this until it returnActive is false
			
			if(rxSerial()){
				if(rxID == MSG_LEFT_CAR_ID){
					if(memcmp(rxLeftID, leftCarID, UID_LEN)==0) {	// check if rx ID matches onboard
						startBlink(LIGHT_BL | LIGHT_BR, LIGHT_BR, 3, 250, LIGHT_BL | LIGHT_BR);	// blink LIGHT_BL 3x
					} else {
						// blink LIGHT_FL 3x? lightsLeftCarMismatch();
					}
				}
				if(rxID == MSG_RIGHT_CAR_ID){
					if(memcmp(rxRightID, rightCarID, UID_LEN)==0) {	// check if rx ID matches onboard
						startBlink(LIGHT_BL | LIGHT_BR, LIGHT_BL, 3, 250, LIGHT_BL | LIGHT_BR);	// blink LIGHT_BR 3x
					} else {
						// blink LIGHT_FR 3x? lightsRightCarMismatch();
					}
				}
			}
			
			if (blinkState.active) {
				updateBlink();
			}
			
			rfidLStat = leftReader.readTag(rfidThresh);				// poll the left RFID reader
			if(rfidLStat == RFID_NEW){
				memcpy(leftCarID, leftReader.uid, UID_LEN);
				txLID 				= true;
			}
			
			if(txLID){
				txStatus lid = txCarID(leftCarID, true); 				// helper function handles transmission status
				switch (lid) {
					case TX_ACKED:										
						txLID 		= false;							// left ID no longer pending
						resetTxState(MSG_LEFT_CAR_ID);
						break;
					case TX_TIMEOUT:
					case TX_FAILED:
						txLID 		= false;							// left ID transmission failed
						resetTxState(MSG_LEFT_CAR_ID);					// reset transmit message
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
			
			rfidRStat = rightReader.readTag(rfidThresh);			// poll the right RFID reader	
			if (rfidRStat == RFID_NEW){
				memcpy(rightCarID, rightReader.uid, UID_LEN);
				txRID 				= true;
			}
			
			if(txRID){
				txStatus rid = txCarID(rightCarID, true); 				// helper function handles transmission status
				switch (rid) {
					case TX_ACKED:										
						txRID 		= false;							// right ID no longer pending
						resetTxState(MSG_RIGHT_CAR_ID);
						break;
					case TX_TIMEOUT:
					case TX_FAILED:
						txRID 		= false;							// right ID transmission failed
						resetTxState(MSG_RIGHT_CAR_ID);					// reset transmit message
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

			updateBlink();
			if (currentState == targetState && !blinkState.active){		
				if (isStartPressed()){									// only check the button if we aren't already processing a state change
					targetState 			= RACE_COUNTDOWN;			// handle a managed transition to countdown state
				}
				if (isModePressed()){
					targetState 			= RACE_IDLE;				// handle a managed transition back to idle state
				}
				
			} else {
				txStatus s = txRaceState(targetState); 					// helper function handles transmission status
				switch (s) {
					case TX_ACKED:										
						currentState 		= targetState;				// commit state change
						resetTxState(MSG_RACE_STATE);
						stateFirstPass 		= true;						// reset first pass marker
						break;
					case TX_TIMEOUT:
					case TX_FAILED:
						targetState			= currentState;				// abandon state transition
						resetTxState(MSG_RACE_STATE);					// reset transmit message
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
			break;
			
		case RACE_COUNTDOWN:	
			if(stateFirstPass){
				// first pass only actions here
				cdState 				= CD_STAGED;
				prevCdState 			= cdState;
				targetState				= currentState;							// clear any target state collision
				rxState					= currentState;							// clear any rxState collision
				stateFirstPass 			= false;	
			}
			
			if(rxSerial()){
				// message specific actions here
			}
			
			// Tick the countdown state.  This function will handle managing stage delays
			// as well as managing the countdown state
			cdState = tickCountdownState(currentMode, cdState);
			
			// Watch for the triggers (given right mode).  Drop the gate but store a foul.
			if (currentMode != MODE_GATEDROP){
				if (isLeftPressed() && gateStatus.leftUp){
					leftStartTime 		= micros();
					leftFoul 			= true;
					dropGate(gateL);
				}
				if (isRightPressed() && gateStatus.rightUp){
					rightStartTime 		= micros();
					rightFoul 			= true;
					dropGate(gateR);
				}
			}
			
			if (cdState != prevCdState){
				if (cdState == CD_GO){
					targetState 		= RACE_RACING;					// when GO has been hit in countdown, trigger a state transition
					resetTxState(MSG_RACE_STATE);
					raceStartTime		= micros();						// when GO has been hit in countdown, tell finishController race is started
					pendStartTx			= true;
					resetTxState(MSG_RACE_START);
					
					if (currentMode == MODE_GATEDROP){
						// The gate drop mode everyone starts at the same time
						leftStartTime 	= raceStartTime;
						rightStartTime 	= raceStartTime;
						dropGate(gateL);
						dropGate(gateR);
					}
				}
				
				byte cdLights	= buildLightConfig(cdState, leftFoul, rightFoul, currentMode);	// set new light pattern
				updateLights(cdLights);									// update lights only when new cdState
				prevCdState = cdState;
			}
			
			if (pendStartTx){
				txStatus strt = txRaceStart(0b0001); 					// helper function handles transmission status
				switch (strt) {
					case TX_ACKED:
						startDelay	= micros();							// Log time delay between actual and ACK start
						pendStartTx 				= false;
						resetTxState(MSG_RACE_START);
						break;
					case TX_TIMEOUT:
					case TX_FAILED:
						pendStartTx 				= false;
						resetTxState(MSG_RACE_START);
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
			
			if (currentState != targetState){
				txStatus s = txRaceState(targetState); 					// helper function handles transmission status
				switch (s) {
					case TX_ACKED:										
						currentState 		= targetState;				// commit state change
						resetTxState(MSG_RACE_STATE);
						stateFirstPass 		= true;						// reset first pass marker
						break;
					case TX_TIMEOUT:
					case TX_FAILED:
						targetState			= currentState;				// abandon state transition
						resetTxState(MSG_RACE_STATE);					// reset transmit message
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
			break;
			
		case RACE_RACING:
			if(stateFirstPass){
				// first pass only actions here
				leftReactionTime				= 0;							// reset reaction time
				rightReactionTime				= 0;							// reset reaction time
				unsigned long raceTimeOffset	= startDelay - raceStartTime;	// not currently used but could compensate for line delays
				foulStatusPending				= true;							// always send foul status
				uint8_t foulMask				= 0;
				if (leftFoul)  foulMask		   |= foul_left;					// add left foul status to mask
				if (rightFoul) foulMask		   |= foul_right;					// add right foul stats to mask
				targetState		= currentState;									// clear any target state collision
				rxState			= currentState;									// clear any rxState collision
				resetTxState(MSG_RACE_START);
				resetTxState(MSG_FOUL);
				resetTxState(MSG_LEFT_REACT);
				resetTxState(MSG_RIGHT_REACT);
				stateFirstPass 					= false;
			}
			
			if(rxSerial()){
				// wait for the race complete state transiiton rx
				if(rxID == MSG_RACE_STATE){
					if (rxState != RACE_COMPLETE){
						// future - send error: bad state transition request
						targetState		= currentState;					// clear state transition
					}
				}
			}
			
			if (currentMode != MODE_GATEDROP){
				// Watch for the triggers (given correct mode).
				if (isLeftPressed() && gateStatus.leftUp){
					leftStartTime 			= micros();
					dropGate(gateL);
					reactLeftPending		= true;
				}
				if (isRightPressed() && gateStatus.rightUp){
					rightStartTime 			= micros();
					dropGate(gateR);
					reactRightPending 		= true;					
				}
				// calculate reaction times, gate drop stays at zero
				if (!gateStatus.leftUp && leftReactionTime == 0){
					if (leftFoul){
						leftReactionTime	= elapsedMicros(raceStartTime, leftStartTime);	// race time is bigger since they started early
					} else {
						leftReactionTime	= elapsedMicros(leftStartTime, raceStartTime);	// normally left start time is bigger
					}
				}
				if (!gateStatus.rightUp && rightReactionTime == 0){
					if (rightFoul){
						rightReactionTime	= elapsedMicros(raceStartTime, rightStartTime);	// race time is bigger since they started early
					} else {
						rightReactionTime	= elapsedMicros(rightStartTime, raceStartTime);	// normally right start time is bigger
					}
				}
			}

			if (!gateStatus.leftUp && !gateStatus.rightUp){
				// only transmit once both tracks have started to avoid detection delays
				if (reactLeftPending){
					txStatus lr = txReactionTime(leftReactionTime,true); 	// helper function handles transmission status
					switch (lr) {
						case TX_ACKED:										
							reactLeftPending		= false;				// transmission complete
							resetTxState(MSG_LEFT_REACT);
							break;
						case TX_TIMEOUT:
						case TX_FAILED:
							reactLeftPending 		= false;				// transmission failed
							resetTxState(MSG_LEFT_REACT);					// reset transmit message
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
			
				if (reactRightPending){
					txStatus rr = txReactionTime(rightReactionTime,false);
					switch (rr) {
						case TX_ACKED:										
							reactRightPending		= false;				// transmission complete
							resetTxState(MSG_RIGHT_REACT);
							break;
						case TX_TIMEOUT:
						case TX_FAILED:
							reactRightPending 		= false;				// transmission failed
							resetTxState(MSG_RIGHT_REACT);					// reset transmit message
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
				if (foulStatusPending){
					txStatus f = txFoulStatus(foulMask);
					switch (f) {
						case TX_ACKED:										
							foulStatusPending		= false;				// transmission complete
							resetTxState(MSG_FOUL);
							break;
						case TX_TIMEOUT:
						case TX_FAILED:
							foulStatusPending 		= false;				// transmission failed
							resetTxState(MSG_FOUL);							// reset transmit message
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
			
			if (!foulStatusPending && !reactLeftPending && !reactRightPending){
				// wait until all pending message have been sent until completing transition
				currentState 				= rxState;			// commit state transition
			}
			break;
			
		case RACE_COMPLETE:
			if(stateFirstPass){
				// first pass only actions here
				stateFirstPass 			= false;
				rxLeftWin				= false;
				rxRightWin				= false;
				rxTie					= false;
				winLightsPend			= false;
				blinkState.active 		= false;  						// Clear any pending blinks
				targetState		= currentState;							// clear any target state collision
				rxState			= currentState;							// clear any rxState collision
			}
			
			if(rxSerial()){
				// message-specific actions
				if(rxID == MSG_WINNER){
					winLightsPend			= true;						// set the winner lights to display	
				}
				if(rxID == MSG_RACE_STATE){
					if (targetState != RACE_IDLE){
						// future - send error: bad state transition request
						targetState		= currentState;					// clear state transition
					}
				}
			}
			
			if (winLightsPend){ 										// Blink lights to show winner
				if(rxLeftWin)	startBlink(LIGHT_GO | LIGHT_FR, LIGHT_FR, 3, 250, LIGHT_GO | LIGHT_FR);
				if(rxRightWin)	startBlink(LIGHT_GO | LIGHT_FL, LIGHT_FL, 3, 250, LIGHT_GO | LIGHT_FL);
				if(rxTie) 		startBlink(LIGHT_GO, 0x00, 3, 250, LIGHT_GO);
				winLightsPend 			= false;
			}
			
			if (isStartPressed() && startReleased){
				startReleased			= false;						// don't revist until button released
				dispAdv					= true;
				resetTxState(MSG_DISP_ADVANCE);
			}
			if (!isStartPressed())		startReleased	= true;			// button released, ready for next detection
			
			if (dispAdv){
					txStatus d = txDisplayAdvance();
					switch (d) {
						case TX_ACKED:										
							dispAdv		= false;						// transmission complete
							resetTxState(MSG_DISP_ADVANCE);
							break;
						case TX_TIMEOUT:
						case TX_FAILED:
							dispAdv 	= false;						// transmission failed
							resetTxState(MSG_DISP_ADVANCE);				// reset transmit message
							break;
						case TX_NONE:
						case TX_SENT:
						case TX_NACKED:
						default:
							break;
					}
				}
				
				if (!winLightsPend && !updateBlink()){				// note: this also executes teh updateBlink() function to process blinks
				// wait until all pending message have been sent until completing transition
				currentState 				= rxState;				// commit state transition
			}			
			break;
			
		case RACE_TEST:  // not currently implemented, return to idle
		default:
			currentState = RACE_IDLE;
			break;
	}
}

// Helper function to manage countdown timing based on race mode
countdownState tickCountdownState(raceMode mode, countdownState cdState){
	unsigned long currentTime = millis();
	switch (cdState) {
		case CD_STAGED:
			if (mode == MODE_PRO){
				stageDelay = 400;
				cdState = CD_Y1;
			} else {
				stageDelay = 500;
				cdState = CD_Y3;
			}
			cdTimer = currentTime;
			break;
		case CD_Y3:
			if (currentTime - cdTimer >= stageDelay){
				cdState = CD_Y2;
				cdTimer = currentTime;
			}
			break;
		case CD_Y2:
			if (currentTime - cdTimer >= stageDelay){
				cdState = CD_Y1;
				cdTimer = currentTime;
			}
			break;
		case CD_Y1:
			if (currentTime - cdTimer >= stageDelay){
				cdState = CD_GO;
				cdTimer = currentTime;
			}
			break;
		default:
			break;
	}
	return cdState;	
}

// Helper function to calculate elapsed microseconds with overflow protection
unsigned long elapsedMicros(unsigned long startTime, unsigned long endTime) {
    if (endTime >= startTime) {
        return endTime - startTime;  					// Normal case
    } else {
        return (0xFFFFFFFF - startTime) + endTime + 1;	// Overflow case
    }
}

// Add state validation
bool isValidStateTransition(raceState from, raceState to) {
    switch(from) {
        case RACE_IDLE:     return (to == RACE_STAGING || to == RACE_TEST);
        case RACE_STAGING:  return (to == RACE_COUNTDOWN || to == RACE_IDLE);
        case RACE_COUNTDOWN:return (to == RACE_RACING || to == RACE_IDLE);
        case RACE_RACING:   return (to == RACE_COMPLETE);
        case RACE_COMPLETE: return (to == RACE_IDLE);
        default: return false;
    }
}