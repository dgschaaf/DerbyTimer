#ifndef serialComm_H
#define serialComm_H
#include "globals.h"

// enumerations
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
	MSG_LEFT_RESULT,		// Race time, reaction time, foul status
	MSG_RIGHT_RESULT,	// Race time, reaction time, foul status
	MSG_FOUL,			// foul status of left and right
	MSG_WINNER, 		// did L or R win for flashing tree lights
	MSG_DISP_ADVANCE, 	// start is pressed, move to reaction display

	
	MSG_COUNT			// keep as last to count the number of messages
};

enum txStatus : uint8_t {
	TX_NONE,			// message not yet sent
	TX_SENT,			// message sent
	TX_ACKED,			// message acknowledged
	TX_TIMEOUT,			// message ACK not received
	TX_NACKED,			// message improperly received
	TX_FAILED,			// message has permanently failed and cannot be sent

	TX_STATUS_COUNT		// keep as last to count the number of statuses
};

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
	
// Setup helper functions
void setupSerial();

// Rx helper functions
bool rxSerial();

// Tx helper functions
txStatus txRaceMode(raceMode newMode);
txStatus txRaceState(raceState newState);
txStatus txRaceStart(uint8_t start);
txStatus txReactionTime(uint32_t reactionTime, bool isLeft);
txStatus txFoulStatus(uint8_t foul);
txStatus txWinner(uint8_t winner);
txStatus txDisplayAdvance();
txStatus txError(errCode err);
void txAck(uint8_t ackID);
void txNack(uint8_t nackID);

// misc helper functions
void sendMessage(serialMsgID id, const uint8_t* data, uint8_t dataLen);
uint8_t getExpectedPayloadLength(serialMsgID id);
void resetTxState(serialMsgID id);

// defintions
#define txTimeout 			50		// milliseconds to wait for tx timeout

// Received Message Variables
extern serialMsgID rxID;					// received message ID
extern serialMsgID lastAckedMsgID;
extern serialMsgID lastNackedMsgID;
extern raceMode rxMode;
extern raceState rxState;
extern errCode lastErrorCode;
extern bool	rxRaceStart;
extern bool	rxLeftStart;
extern bool	rxRightStart;
extern bool	rxLeftFoul;
extern bool	rxRightFoul;					
extern bool	rxLeftWin;
extern bool	rxRightWin;
extern bool	rxTie;
extern bool rxDisplayAdvanceFlag;
extern int32_t rxLeftReactionTime;
extern int32_t rxRightReactionTime;

#endif