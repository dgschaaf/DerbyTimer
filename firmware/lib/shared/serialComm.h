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
	TX_NACKED,			// message improperly received -- triggers retry
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
// The single mailbox between the wire and both controllers: rxSerial() is the
// only writer of record, controller code reads. Fields are grouped by
// LIFECYCLE, because the groups are not interchangeable -- reading a
// level-triggered value as though it were an event re-fires it forever, which
// is the class of bug that has bitten this struct before. Check the banner
// above a field before using it.
//
// Mode and State are stored as uint8_t -- callers cast to raceMode/raceState
// from raceTypes.h. Nothing here goes on the wire and nothing initializes it
// positionally, so grouping by lifecycle costs only alignment padding: the two
// int32_t reaction times sit mid-struct because they belong to their group,
// which is worth 8 bytes of padding on a 4-byte-aligned target (none on AVR).
struct SerialRxState {

	// ---- LEVEL-TRIGGERED -- holds its last value forever; never an event ----
	// Nothing ever clears these. They answer "what did the peer last say?",
	// never "did something just happen?". To act once on a change, pair one
	// with an edge flag (see StateChanged) or with a guard of your own.
	serialMsgID ID              = MSG_NULL;  // set: rxSerial, on every message. Used within the same pass to address the ACK; no controller reads it.
	uint8_t     Mode            = 0;         // set: rxSerial on MSG_RACE_MODE (cast to raceMode). The SC also assigns this locally to mirror its own
	                                         // confirmed mode, so on the SC it is not purely "what the peer sent" -- a known wart, not a contract.
	uint8_t     State           = 0;         // set: rxSerial on MSG_RACE_STATE (cast to raceState). Safe for "where is the peer"; NEVER to drive a
	                                         // transition -- that is StateChanged's job.
	serialMsgID lastAckedMsgID  = MSG_NULL;  // set: rxSerial on MSG_ACK, then used in the same pass to mark txState TX_ACKED. Diagnostic afterwards.
	serialMsgID lastNackedMsgID = MSG_NULL;  // set: rxSerial on MSG_NACK; same single-pass use, marking TX_NACKED.

	// ---- EDGE FLAG -- set by rxSerial, consumed exactly once ----
	bool StateChanged = false;   // set: rxSerial on MSG_RACE_STATE. Cleared by: stateMachine::service() (via serviceRx), exactly once.
	                             // rx.State alone is level-triggered (holds its value forever) -- feeding it to a transition
	                             // directly re-fires stale transitions (e.g. RACING->IDLE mid-heat from the previous heat's
	                             // IDLE). Never bypass the flag.

	// ---- CONSUMER-CLEARED -- one named handler reads AND clears ----
	// Safe to treat as events, but only from the designated consumer: whoever
	// reads one owes it a clear in the same breath. clearHeatEvents() sweeps
	// them again at IDLE entry as a backstop, not as the owner.
	errCode lastErrorCode      = err_NULL;  // set: rxSerial on MSG_ERROR. Cleared by: FC main loop, which then forceIdles (abort signal, ADR-0004).
	                                        // The SC deliberately does not consume it (ADR-0004 records the asymmetry), so on the SC it never clears.
	bool    LeftReactionValid  = false;     // set: rxSerial on MSG_LEFT_REACT.  Cleared by: FC handleRxReaction, together with LeftReactionTime.
	bool    RightReactionValid = false;     // set: rxSerial on MSG_RIGHT_REACT. Cleared by: FC handleRxReaction, together with RightReactionTime.
	int32_t LeftReactionTime   = 0;         // payload for the flag above; meaningful only while LeftReactionValid is set.
	int32_t RightReactionTime  = 0;         // payload for the flag above; meaningful only while RightReactionValid is set.
	bool    LeftFoul           = false;     // set: rxSerial on MSG_FOUL -- ASSIGNED from the mask, both lanes at once, so a later MSG_FOUL overwrites
	                                        // both. Cleared by: FC handleRxReaction, which latches the foul into timingInputs.
	bool    RightFoul          = false;     // set and cleared exactly as LeftFoul.
	bool    DisplayAdvanceFlag = false;     // set: rxSerial on MSG_DISP_ADVANCE. Cleared by: FC handleComplete, on every pass that observes it.

	// ---- HEAT-CLEARED -- lives one heat; only clearHeatEvents() clears it ----
	// These stay set after their consumer reads them, on purpose: a heat's
	// outcome must remain readable for the rest of that heat. Consumers are
	// therefore responsible for not re-acting (see the FC's startUs == 0 guard).
	bool RaceStart      = false;   // set: rxSerial on MSG_RACE_START. Read: FC starts heat timing once, guarded.
	bool LeftWin        = false;   // set: rxSerial on MSG_WINNER (winner_leftWin).  Read: SC win-light pattern.
	bool RightWin       = false;   // set: rxSerial on MSG_WINNER (winner_rightWin). Read: SC win-light pattern.
	bool Tie            = false;   // set: rxSerial on MSG_WINNER (winner_tie).      Read: SC win-light pattern.
	bool NoResult       = false;   // set: rxSerial on MSG_WINNER (winner_noResult). Double-foul: SC shows no win lights.
	bool WinnerReceived = false;   // set: rxSerial on ANY MSG_WINNER. The SC's "results are in" gate.

	// ---- DIAGNOSTIC -- never triggers behavior ----
	errCode lastLocalError = err_NULL;  // set: rxSerial on a locally detected parse problem (unknown ID, stale partial,
	                                    // out-of-range payload). Never cleared and never read by controller logic.
	                                    // Must never trigger an abort -- that is lastErrorCode's job.

	// Owns the HEAT-CLEARED group outright, and re-clears the CONSUMER-CLEARED
	// group as a backstop for a handler that never ran. Deliberately leaves the
	// LEVEL-TRIGGERED fields alone (peer status, not heat data), along with
	// StateChanged (owned by the state machine) and lastLocalError.
	// Called at IDLE entry on both controllers.
	void clearHeatEvents() {
		// HEAT-CLEARED: this is the only place these are reset.
		RaceStart          = false;
		LeftWin            = false;
		RightWin           = false;
		Tie                = false;
		NoResult           = false;
		WinnerReceived     = false;
		// CONSUMER-CLEARED: backstop only -- the named handler clears these first.
		LeftFoul           = false;
		RightFoul          = false;
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

// Re-enqueue a message from the payload captured at its original enqueue --
// retry after TX_TIMEOUT/TX_FAILED without rebuilding (or retaining) the
// payload. Returns true if rearmed or re-enqueued; false if the message is
// in flight, queued-but-unsent, never sent, or ended TX_ACKED (resend is
// for failures only). If the failed entry has not been dequeued yet it is
// rearmed in place, so a resend issued in the same loop pass that detected
// the failure still works. See the ADR-0003 amendment.
bool txResend(serialMsgID id);

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
