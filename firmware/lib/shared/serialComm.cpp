#include <Arduino.h>
#include "raceTypes.h"
#include "serialComm.h"

SerialRxState rx;
static const uint8_t maxRetries = 3;

// ---------------------------------------------------------------
// TX tracker: one slot per message ID, holds status + captured payload
// ---------------------------------------------------------------
struct TxTracker {
	txStatus      status;
	unsigned long sendTime;
	uint8_t       retries;
	uint8_t       payload[4];   // max payload: sizeof(uint32_t)
	uint8_t       payloadLen;
};

static TxTracker   txState[MSG_COUNT];   // zero-initialized (static storage)
static serialMsgID txQueue[MSG_COUNT];   // FIFO of pending message IDs
static uint8_t     txQueueLen = 0;

// ---------------------------------------------------------------
// Setup
// ---------------------------------------------------------------
void setupSerial() {
	Serial.begin(115200);
}

// ---------------------------------------------------------------
// RX
// ---------------------------------------------------------------
bool rxSerial() {
	if (Serial.available() < 1) return false;

	serialMsgID id = (serialMsgID)Serial.peek();
	uint8_t expectedLen = getExpectedPayloadLength(id);
	uint8_t available   = Serial.available();

	if (id >= MSG_COUNT) {
		Serial.read();
		return false;
	}
	if (available < (1 + expectedLen)) return false;

	rx.ID = (serialMsgID)Serial.read();

	switch (rx.ID) {
		case MSG_RACE_MODE: {
			if (Serial.available() >= 1) {
				rx.Mode = Serial.read();
				txAck(rx.ID);
			}
			break;
		}
		case MSG_RACE_STATE: {
			if (Serial.available() >= 1) {
				rx.State = Serial.read();
				txAck(rx.ID);
			}
			break;
		}
		case MSG_RACE_START: {
			if (Serial.available() >= 1) {
				uint8_t startMask = Serial.read();
				rx.RaceStart  = startMask & 0b0001;
				rx.LeftStart  = startMask & 0b0010;
				rx.RightStart = startMask & 0b0100;
				txAck(rx.ID);
			}
			break;
		}
		case MSG_LEFT_REACT:
		case MSG_RIGHT_REACT: {
			if (Serial.available() >= sizeof(uint32_t)) {
				int32_t reaction;
				Serial.readBytes((uint8_t*)&reaction, sizeof(reaction));
				if (rx.ID == MSG_LEFT_REACT) {
					rx.LeftReactionTime  = reaction;
					rx.LeftReactionValid = true;
				} else {
					rx.RightReactionTime  = reaction;
					rx.RightReactionValid = true;
				}
				txAck(rx.ID);
			}
			break;
		}
		case MSG_FOUL: {
			if (Serial.available() >= 1) {
				uint8_t foulMask = Serial.read();
				rx.LeftFoul  = foulMask & 0b0001;
				rx.RightFoul = foulMask & 0b0010;
				txAck(rx.ID);
			}
			break;
		}
		case MSG_WINNER: {
			if (Serial.available() >= 1) {
				uint8_t winnerMask = Serial.read();
				rx.LeftWin        = winnerMask & 0b0001;
				rx.RightWin       = winnerMask & 0b0010;
				rx.Tie            = winnerMask & 0b0100;
				rx.NoResult       = winnerMask & 0b1000;
				rx.WinnerReceived = true;
				txAck(rx.ID);
			}
			break;
		}
		case MSG_DISP_ADVANCE: {
			rx.DisplayAdvanceFlag = true;
			txAck(rx.ID);
			break;
		}
		case MSG_ACK: {
			if (Serial.available() >= 1) {
				rx.lastAckedMsgID = (serialMsgID)Serial.read();
				if (rx.lastAckedMsgID < MSG_COUNT) {
					txState[rx.lastAckedMsgID].status = TX_ACKED;
				}
			}
			break;
		}
		case MSG_NACK: {
			if (Serial.available() >= 1) {
				rx.lastNackedMsgID = (serialMsgID)Serial.read();
				if (rx.lastNackedMsgID < MSG_COUNT) {
					txState[rx.lastNackedMsgID].status = TX_NACKED;
				}
			}
			break;
		}
		case MSG_ERROR: {
			if (Serial.available() >= 1) {
				rx.lastErrorCode = (errCode)Serial.read();
				txAck(rx.ID);
			}
			break;
		}
		default: {
			txNack(rx.ID);
			break;
		}
	}
	return true;
}

// ---------------------------------------------------------------
// TX internal engine
// ---------------------------------------------------------------

// Adds id to the FIFO queue, capturing payload at enqueue time.
// Returns false if already in flight (TX_SENT) or already in queue.
// Priority messages are inserted at the front.
static bool enqueueMsg(serialMsgID id, const uint8_t* payload, uint8_t len, bool priority) {
	if (id >= MSG_COUNT) return false;
	if (txState[id].status == TX_SENT) return false;
	for (uint8_t i = 0; i < txQueueLen; i++) {
		if (txQueue[i] == id) return false;
	}
	memset(&txState[id], 0, sizeof(TxTracker));
	if (payload && len > 0) {
		uint8_t n = (len <= sizeof(txState[id].payload)) ? len : sizeof(txState[id].payload);
		memcpy(txState[id].payload, payload, n);
		txState[id].payloadLen = n;
	}
	if (priority && txQueueLen > 0) {
		memmove(&txQueue[1], &txQueue[0], txQueueLen * sizeof(serialMsgID));
		txQueue[0] = id;
	} else {
		txQueue[txQueueLen] = id;
	}
	txQueueLen++;
	return true;
}

// Advances the front-of-queue message through its state machine.
// Dequeues on terminal status (TX_ACKED, TX_TIMEOUT, TX_FAILED).
static void txDrive() {
	if (txQueueLen == 0) return;
	serialMsgID   id  = txQueue[0];
	TxTracker&    s   = txState[id];
	unsigned long now = millis();

	switch (s.status) {
		case TX_NONE:
		case TX_NACKED:
			if (s.retries > maxRetries) { s.status = TX_FAILED; return; }
			sendMessage(id, s.payloadLen > 0 ? s.payload : nullptr, s.payloadLen);
			s.sendTime = now;
			s.retries++;
			s.status   = TX_SENT;
			return;
		case TX_SENT:
			if (now - s.sendTime >= txTimeout) s.status = TX_TIMEOUT;
			return;
		case TX_ACKED:
		case TX_TIMEOUT:
		case TX_FAILED:
			memmove(&txQueue[0], &txQueue[1], (txQueueLen - 1) * sizeof(serialMsgID));
			txQueueLen--;
			return;
	}
}

// ---------------------------------------------------------------
// TX public API — enqueue and return bool
// ---------------------------------------------------------------

bool txRaceMode(uint8_t newMode) {
	return enqueueMsg(MSG_RACE_MODE, &newMode, 1, false);
}

bool txRaceState(uint8_t newState) {
	return enqueueMsg(MSG_RACE_STATE, &newState, 1, false);
}

bool txRaceStart(uint8_t start) {
	return enqueueMsg(MSG_RACE_START, &start, 1, true);   // priority — jumps queue
}

bool txReactionTime(uint32_t reactionTime, Lane lane) {
	serialMsgID id = (lane == LANE_LEFT) ? MSG_LEFT_REACT : MSG_RIGHT_REACT;
	uint8_t payload[sizeof(uint32_t)];
	memcpy(payload, &reactionTime, sizeof(uint32_t));
	return enqueueMsg(id, payload, sizeof(uint32_t), false);
}

bool txFoulStatus(uint8_t foul) {
	return enqueueMsg(MSG_FOUL, &foul, 1, false);
}

bool txWinner(uint8_t winner) {
	return enqueueMsg(MSG_WINNER, &winner, 1, false);
}

bool txDisplayAdvance() {
	return enqueueMsg(MSG_DISP_ADVANCE, nullptr, 0, false);
}

bool txError(errCode err) {
	uint8_t e = (uint8_t)err;
	return enqueueMsg(MSG_ERROR, &e, 1, false);
}

txStatus txStatusOf(serialMsgID id) {
	if (id >= MSG_COUNT) return TX_NONE;
	return txState[id].status;
}

void txService() {
	txDrive();
}

// ---------------------------------------------------------------
// Immediate responses — fire and forget, never queued
// ---------------------------------------------------------------

void txAck(uint8_t ackID) {
	Serial.write((uint8_t)MSG_ACK);
	Serial.write(ackID);
}

void txNack(uint8_t nackID) {
	Serial.write((uint8_t)MSG_NACK);
	Serial.write(nackID);
}

// ---------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------

void sendMessage(serialMsgID id, const uint8_t* data, uint8_t dataLen) {
	Serial.write((uint8_t)id);
	if (dataLen > 0 && data != nullptr) {
		Serial.write(data, dataLen);
	}
}

uint8_t getExpectedPayloadLength(serialMsgID id) {
	switch (id) {
		case MSG_RACE_MODE:
		case MSG_RACE_STATE:
		case MSG_RACE_START:
		case MSG_FOUL:
		case MSG_WINNER:
		case MSG_ACK:
		case MSG_NACK:
		case MSG_ERROR:
			return 1;
		case MSG_LEFT_REACT:
		case MSG_RIGHT_REACT:
			return sizeof(uint32_t);
		case MSG_DISP_ADVANCE:
			return 0;
		default:
			return 0;
	}
}
