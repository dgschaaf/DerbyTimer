#ifndef serialComm_H
#define serialComm_H

#include "raceTypes.h"   // Lane used in txReactionTime signature

// -------------------- Wire Protocol Bitmasks --------------------
// These define how flags are packed into message payloads.
// Winner event codes (used in MSG_WINNER payload)
#define winner_leftWin  0b0001
#define winner_rightWin 0b0010
#define winner_tie      0b0100
#define winner_noResult 0b1000
// Foul event codes (used in MSG_FOUL payload)
#define foul_left       0b0001
#define foul_right      0b0010
#define foul_both       0b0011

// -------------------- Message IDs --------------------
enum serialMsgID : uint8_t {
	MSG_NULL,			// empty id, used as initialization placeholder
	MSG_ACK, 			// acknowledge
	MSG_NACK, 			// not acknowledge
    MSG_RACE_MODE,  	// race mode change (idle only)
	MSG_RACE_STATE, 	// state change
    MSG_RACE_START, 	// start race, finish starts timer
    MSG_ERROR, 			// any error states
	MSG_LEFT_REACT, 	// reaction time and foul status
	MSG_RIGHT_REACT, 	// reaction time and foul status
	MSG_FOUL,			// foul status of left and right
	MSG_WINNER, 		// did L or R win for flashing tree lights
	MSG_DISP_ADVANCE, 	// start is pressed, move to reaction display

	MSG_COUNT			// keep as last to count the number of messages
};

// -------------------- TX Status --------------------
enum txStatus : uint8_t {
	TX_NONE,			// message not yet sent
	TX_SENT,			// message sent, awaiting ACK
	TX_ACKED,			// message acknowledged
	TX_TIMEOUT,			// ACK not received within txTimeout
	TX_NACKED,			// message improperly received — triggers retry
	TX_FAILED,			// permanently failed (too many NACKs)

	TX_STATUS_COUNT		// keep as last to count the number of statuses
};

// -------------------- Error Codes --------------------
enum errCode : uint8_t {
	err_NULL,				// empty error, used as initialization placeholder
	err_STATE_TX_TIMEOUT,	// State transition message timeout
    err_MODE_TX_TIMEOUT,	// Mode change timeout
    err_START_TX_TIMEOUT,	// Race start signal timeout
    err_SERIAL_OVERFLOW,	// Serial buffer overflow
    err_INVALID_MSG,		// Received corrupted message
    err_STATE_MISMATCH,		// Controllers in different states

	err_Count				// keep as last to count the number of errors
};

// -------------------- Global RX State --------------------
// Updated by rxSerial() each loop. Mode and State are stored as
// uint8_t — callers cast to raceMode/raceState from raceTypes.h.
struct SerialRxState {
	serialMsgID ID              = MSG_NULL;  // last received message ID
	serialMsgID lastAckedMsgID  = MSG_NULL;  // last acknowledged message ID
	serialMsgID lastNackedMsgID = MSG_NULL;  // last not acknowledged message ID
	uint8_t     Mode            = 0;         // last received race mode (MODE_GATEDROP -- cast to raceMode)
	uint8_t     State           = 0;         // last received race state (RACE_IDLE -- cast to raceState)
	bool        StateChanged    = false;     // set when MSG_RACE_STATE arrives; consumed once by stateMachine::serviceRx().
	                                         // rx.State alone is level-triggered (holds its value forever) -- feeding it
	                                         // to rxTransition() directly re-fires stale transitions (e.g. RACING->IDLE
	                                         // mid-heat from the previous heat's IDLE). Never bypass the flag.
	errCode     lastErrorCode   = err_NULL;  // last received error code (MSG_ERROR from the peer -- FC treats any
	                                         // value here as a critical abort signal, see ADR-0004)
	errCode     lastLocalError  = err_NULL;  // last locally detected RX parse problem (unknown ID, stale partial,
	                                         // out-of-range payload). Diagnostic only -- must never trigger aborts.

	bool    RaceStart           = false;
	bool    LeftFoul            = false;
	bool    RightFoul           = false;
	bool    LeftWin             = false;
	bool    RightWin            = false;
	bool    Tie                 = false;
	bool    NoResult            = false;   // double-foul / no result (winner_noResult)
	bool    WinnerReceived      = false;   // set when any MSG_WINNER lands, cleared at race start
	bool    DisplayAdvanceFlag  = false;
	bool    LeftReactionValid   = false;
	bool    RightReactionValid  = false;
	int32_t LeftReactionTime    = 0;
	int32_t RightReactionTime   = 0;

	void clearHeatEvents() {
		RaceStart          = false;
		LeftFoul           = false;
		RightFoul          = false;
		LeftWin            = false;
		RightWin           = false;
		Tie                = false;
		NoResult           = false;
		WinnerReceived     = false;
		DisplayAdvanceFlag = false;
		LeftReactionValid  = false;
		RightReactionValid = false;
		LeftReactionTime   = 0;
		RightReactionTime  = 0;
	}
};
extern SerialRxState rx;

// -------------------- Public API --------------------
constexpr unsigned long serialBaud = 115200;

// SC: protocol runs on the primary UART (Serial) -- begin() handled inside.
void setupSerial();
// FC: protocol runs on a caller-initialized port (e.g. Serial1 on the Nano 33
// BLE, freeing USB Serial for debug output). Call port.begin(serialBaud) first.
// Stream comes from Arduino.h (or the native test mock) -- include it first.
void setupSerialBus(Stream& port);
bool rxSerial();

// Enqueue outgoing messages -- returns true if newly enqueued, false if already in flight or queued.
// Callers enqueue once and move on; query outcome with txStatusOf().
bool txRaceMode(uint8_t newMode);
bool txRaceState(uint8_t newState);
bool txRaceStart();                                      // zero payload; priority -- jumps to front of queue
bool txReactionTime(uint32_t reactionTime, Lane lane);
bool txFoulStatus(uint8_t foul);
bool txWinner(uint8_t winner);
bool txDisplayAdvance();
bool txError(errCode err);

// Query the current status of any outgoing message slot.
txStatus txStatusOf(serialMsgID id);

// Drive the TX queue -- call once per main loop.
void txService();

// Immediate responses -- fire and forget, not queued.
void txAck(uint8_t ackID);
void txNack(uint8_t nackID);

// -------------------- Helpers --------------------
void    sendMessage(serialMsgID id, const uint8_t* data, uint8_t dataLen);
uint8_t getExpectedPayloadLength(serialMsgID id);

// TX timing
constexpr uint16_t txTimeout = 50;  // milliseconds to wait for ACK before TX_TIMEOUT

// RX validation
constexpr uint16_t stalePartialTimeoutMs = 100;        // flush a partial message whose payload never arrives
constexpr uint32_t maxValidReactionUs    = 10000000UL; // plausibility bound for received reaction times
                                                       // (matches FC sensors config.maxRaceTimeUs)

#endif  // serialComm_H
