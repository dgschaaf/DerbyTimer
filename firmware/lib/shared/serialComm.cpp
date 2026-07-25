#include <Arduino.h>
#include "raceTypes.h"
#include "serialComm.h"

SerialRxState rx;
static const uint8_t maxRetries = 3;   // retries after the initial send -- 4 transmission attempts total
                                       // (txDrive compares retries > maxRetries after incrementing per send)

// ---------------------------------------------------------------
// TX tracker: one slot per message ID in txState[], holding the message's
// lifecycle status (TX_NONE -> TX_SENT -> ACKED/NACKED/TIMEOUT/FAILED) plus
// a payload copy captured at enqueue time, so the wire always carries the
// value current when the event happened. txDrive() walks the front of
// txQueue[] through this state machine; one message in flight at a time.
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
// All protocol I/O goes through `bus` so each controller can pick its port:
// SC uses Serial (only UART on the ATmega328P); FC uses Serial1 (hardware
// UART on D0/D1), keeping USB Serial free for debug output (P2-37).
static Stream* bus = &Serial;

void setupSerial() {
	Serial.begin(serialBaud);
	bus = &Serial;
}

void setupSerialBus(Stream& port) {
	bus = &port;
}

// ---------------------------------------------------------------
// RX
// ---------------------------------------------------------------
bool rxSerial() {
	// Stale-partial tracker: a peeked ID whose payload never arrives (tail
	// lost on the wire) would block the head of the stream forever.
	static serialMsgID   stalledID    = MSG_COUNT;   // MSG_COUNT = no stall in progress
	static unsigned long stalledSince = 0;

	if (bus->available() < 1) {
		stalledID = MSG_COUNT;
		return false;
	}

	serialMsgID id = (serialMsgID)bus->peek();
	uint8_t expectedLen = getExpectedPayloadLength(id);
	uint8_t available   = bus->available();

	if (id >= MSG_COUNT) {
		bus->read();
		rx.lastLocalError = err_INVALID_MSG;
		stalledID = MSG_COUNT;
		return false;
	}
	if (available < (1 + expectedLen)) {
		// Payload not all here yet. Normal case: it completes within a few ms.
		// If it stalls past stalePartialTimeoutMs, drop the ID byte and NACK so
		// the sender's retry can resync the stream.
		if (stalledID != id) {
			stalledID    = id;
			stalledSince = millis();
		} else if (millis() - stalledSince >= stalePartialTimeoutMs) {
			bus->read();
			txNack(id);
			rx.lastLocalError = err_INVALID_MSG;
			stalledID = MSG_COUNT;
		}
		return false;
	}
	stalledID = MSG_COUNT;

	rx.ID = (serialMsgID)bus->read();

	switch (rx.ID) {
		case MSG_RACE_MODE: {
			if (bus->available() >= 1) {
				rx.Mode = bus->read();
				txAck(rx.ID);
			}
			break;
		}
		case MSG_RACE_STATE: {
			if (bus->available() >= 1) {
				rx.State        = bus->read();
				rx.StateChanged = true;
				txAck(rx.ID);
			}
			break;
		}
		case MSG_RACE_START: {
			rx.RaceStart = true;
			txAck(rx.ID);
			break;
		}
		case MSG_LEFT_REACT:
		case MSG_RIGHT_REACT: {
			if (bus->available() >= (int)sizeof(uint32_t)) {
				int32_t reaction;
				bus->readBytes((uint8_t*)&reaction, sizeof(reaction));
				if (reaction < 0 || (uint32_t)reaction > maxValidReactionUs) {
					// Implausible value -- likely wire corruption. NACK so the
					// sender retries; do not let it reach the display.
					rx.lastLocalError = err_INVALID_MSG;
					txNack(rx.ID);
					break;
				}
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
			if (bus->available() >= 1) {
				uint8_t foulMask = bus->read();
				rx.LeftFoul  = foulMask & 0b0001;
				rx.RightFoul = foulMask & 0b0010;
				txAck(rx.ID);
			}
			break;
		}
		case MSG_WINNER: {
			if (bus->available() >= 1) {
				uint8_t winnerMask = bus->read();
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
			if (bus->available() >= 1) {
				rx.lastAckedMsgID = (serialMsgID)bus->read();
				if (rx.lastAckedMsgID < MSG_COUNT) {
					txState[rx.lastAckedMsgID].status = TX_ACKED;
				}
			}
			break;
		}
		case MSG_NACK: {
			if (bus->available() >= 1) {
				rx.lastNackedMsgID = (serialMsgID)bus->read();
				if (rx.lastNackedMsgID < MSG_COUNT) {
					txState[rx.lastNackedMsgID].status = TX_NACKED;
				}
			}
			break;
		}
		case MSG_ERROR: {
			if (bus->available() >= 1) {
				rx.lastErrorCode = (errCode)bus->read();
				txAck(rx.ID);
			}
			break;
		}
		default: {
			rx.lastLocalError = err_INVALID_MSG;
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
		default:
			return;
	}
}

// ---------------------------------------------------------------
// TX public API -- enqueue and return bool
// ---------------------------------------------------------------

bool txRaceMode(uint8_t newMode) {
	return enqueueMsg(MSG_RACE_MODE, &newMode, 1, false);
}

bool txRaceState(uint8_t newState) {
	return enqueueMsg(MSG_RACE_STATE, &newState, 1, false);
}

bool txRaceStart() {
	return enqueueMsg(MSG_RACE_START, nullptr, 0, true);   // priority -- jumps queue
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

bool txResend(serialMsgID id) {
	if (id >= MSG_COUNT) return false;
	TxTracker& s = txState[id];
	if (s.status != TX_TIMEOUT && s.status != TX_FAILED) return false;

	// Failure detected before txDrive() dequeued the terminal entry: it is
	// still at the front of the queue with its payload intact, so rearm it
	// in place. (Re-enqueueing here would be rejected as a duplicate.)
	for (uint8_t i = 0; i < txQueueLen; i++) {
		if (txQueue[i] == id) {
			s.status  = TX_NONE;
			s.retries = 0;
			return true;
		}
	}

	// Already dequeued: re-enqueue from the captured payload. Copy it out
	// first -- enqueueMsg() zeroes the tracker slot before copying in.
	uint8_t payload[sizeof(s.payload)];
	uint8_t len = s.payloadLen;
	memcpy(payload, s.payload, sizeof(payload));
	return enqueueMsg(id, len > 0 ? payload : nullptr, len, id == MSG_RACE_START);
}

txStatus txStatusOf(serialMsgID id) {
	if (id >= MSG_COUNT) return TX_NONE;
	return txState[id].status;
}

void txService() {
	txDrive();
}

// ---------------------------------------------------------------
// Immediate responses -- fire and forget, never queued
// ---------------------------------------------------------------

void txAck(uint8_t ackID) {
	bus->write((uint8_t)MSG_ACK);
	bus->write(ackID);
}

void txNack(uint8_t nackID) {
	bus->write((uint8_t)MSG_NACK);
	bus->write(nackID);
}

// ---------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------

void sendMessage(serialMsgID id, const uint8_t* data, uint8_t dataLen) {
	bus->write((uint8_t)id);
	if (dataLen > 0 && data != nullptr) {
		bus->write(data, dataLen);
	}
}

uint8_t getExpectedPayloadLength(serialMsgID id) {
	switch (id) {
		case MSG_RACE_MODE:
		case MSG_RACE_STATE:
		case MSG_FOUL:
		case MSG_WINNER:
		case MSG_ACK:
		case MSG_NACK:
		case MSG_ERROR:
			return 1;
		case MSG_LEFT_REACT:
		case MSG_RIGHT_REACT:
			return sizeof(uint32_t);
		case MSG_RACE_START:
		case MSG_DISP_ADVANCE:
			return 0;
		default:
			return 0;
	}
}
