#ifndef serialComm_H
#define serialComm_H

// -------------------- Wire Protocol Bitmasks --------------------
// These define how flags are packed into message payloads.
// Winner event codes (used in MSG_WINNER payload)
#define winner_leftWin  0b0001
#define winner_rightWin 0b0010
#define winner_tie      0b0100
// Start event codes (used in MSG_RACE_START payload)
#define start_race      0b0001
#define start_left      0b0010
#define start_right     0b0100
#define start_all       0b0111
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
	TX_SENT,			// message sent
	TX_ACKED,			// message acknowledged
	TX_TIMEOUT,			// message ACK not received
	TX_NACKED,			// message improperly received
	TX_FAILED,			// message has permanently failed and cannot be sent

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
	uint8_t     Mode            = 0;         // last received race mode (MODE_GATEDROP — cast to raceMode)
	uint8_t     State           = 0;         // last received race state (RACE_IDLE — cast to raceState)
	errCode     lastErrorCode   = err_NULL;  // last received error code

	bool    RaceStart           = false;
	bool    LeftStart           = false;
	bool    RightStart          = false;
	bool    LeftFoul            = false;
	bool    RightFoul           = false;
	bool    LeftWin             = false;
	bool    RightWin            = false;
	bool    Tie                 = false;
	bool    DisplayAdvanceFlag  = false;
	int32_t LeftReactionTime    = 0;
	int32_t RightReactionTime   = 0;
};
extern SerialRxState rx;

// -------------------- Public API --------------------
void setupSerial();
bool rxSerial();

txStatus txRaceMode(uint8_t newMode);      // pass (uint8_t)currentMode
txStatus txRaceState(uint8_t newState);    // pass (uint8_t)newState
txStatus txRaceStart(uint8_t start);
txStatus txReactionTime(uint32_t reactionTime, bool isLeft);
txStatus txFoulStatus(uint8_t foul);
txStatus txWinner(uint8_t winner);
txStatus txDisplayAdvance();
txStatus txError(errCode err);

void txAck(uint8_t ackID);
void txNack(uint8_t nackID);

// -------------------- Helpers --------------------
void    sendMessage(serialMsgID id, const uint8_t* data, uint8_t dataLen);
uint8_t getExpectedPayloadLength(serialMsgID id);
void    resetTxState(serialMsgID id);

// TX timing
constexpr uint16_t txTimeout = 50;  // milliseconds to wait for tx timeout

#endif  // serialComm_H
