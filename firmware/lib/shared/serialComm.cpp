#include "serialComm.h"
// if you put the ack into the helper function and make it a bool return, 
// does it cause the code to hang waiting for a response?


serialMsgID lastAckedMsgID			= MSG_NULL;	// initialize to the null message
serialMsgID lastNackedMsgID			= MSG_NULL;	// initialize to the null message
static const uint8_t maxRetries 	= 3;		// number of tx retries allowed

struct TxTracker {
	txStatus status;	
	unsigned long sendTime;
	uint8_t retries;
};
TxTracker txState[MSG_COUNT + 1];  // Indexed by message ID

void setupSerial(){
	Serial.begin(115200);
}

// ************** RX Messages **************
bool rxSerial() {
	if (Serial.available() < 1) return false;
	
	// Check message validity
    serialMsgID id = (serialMsgID)Serial.peek();
	uint8_t expectedLen = getExpectedPayloadLength(id);
    uint8_t available = Serial.available();
	// Invalid ID - clear one byte and try again
    if (id >= MSG_COUNT) {									
        Serial.read();
        return false;
    }
	// Peek at the ID and determine if the entire payload has been received before processing
	if (available < (1 + expectedLen)) return false;

	rxID = (serialMsgID)Serial.read();  // Read 1-byte message ID

	switch (rxID) {
		case MSG_RACE_MODE: {
			if (Serial.available() >= 1) {
				uint8_t newMode = Serial.read();
				rxMode = newMode;  // Update your race mode
				txAck(rxID);
			}
			break;
		}
		case MSG_RACE_STATE: {
			if (Serial.available() >= 1) {
				uint8_t newState = Serial.read();
				rxState = newState;  // Update your global state
				txAck(rxID);
			}
			break;
		}
		case MSG_LEFT_CAR_ID:
		case MSG_RIGHT_CAR_ID: {
			if (Serial.available() >= serialUIDLength) {
				uint8_t uid[serialUIDLength];
				Serial.readBytes(uid, serialUIDLength);
				if (rxID == MSG_LEFT_CAR_ID) {
					memcpy(rxLeftID, uid, serialUIDLength);  // leftCarID must be uint8_t[4]
				} else {
					memcpy(rxRightID, uid, serialUIDLength);
				}
				txAck(rxID);
			}
			break;
		}
		case MSG_RACE_START: {
			if (Serial.available() >= 1) {
				uint8_t startMask 	= Serial.read();
				rxRaceStart 		= startMask & 0b0001;
				rxLeftStart		 	= startMask & 0b0010;
				rxRightStart	 	= startMask & 0b0100;
				txAck(rxID);
			}
			break;
		}
		case MSG_LEFT_REACT:
		case MSG_RIGHT_REACT: {
			if (Serial.available() >= sizeof(uint32_t)) {
				int32_t reaction;
				Serial.readBytes((uint8_t*)&reaction, sizeof(reaction));
				if (rxID == MSG_LEFT_REACT) {
					rxLeftReactionTime = reaction;
				} else {
					rxRightReactionTime = reaction;
				}
				txAck(rxID);
			}
			break;
		}
		case MSG_FOUL: {
			if (Serial.available() >= 1) {
				uint8_t foulMask 	= Serial.read(); // Bit 0 = L, Bit 1 = R
				rxLeftFoul  		= foulMask & 0b0001;
				rxRightFoul 		= foulMask & 0b0010;
				txAck(rxID);
			}
			break;
		}
		case MSG_WINNER: {
			if (Serial.available() >= 1) {
				uint8_t winnerMask	= Serial.read();
				rxLeftWin 			= winnerMask & 0b0001;
				rxRightWin 			= winnerMask & 0b0010;
				rxTie 				= winnerMask & 0b0100;
				txAck(rxID);
			}
			break;
		}
		case MSG_DISP_ADVANCE: {
			rxDisplayAdvanceFlag = true;  // Flag your logic uses elsewhere
			txAck(rxID);
			break;
		}
		case MSG_ACK: {
			if (Serial.available() >= 1) {
				lastAckedMsgID = (serialMsgID)Serial.read(); // used to mark tx message as received
				if (lastAckedMsgID < MSG_COUNT){
					txState[lastAckedMsgID].status = TX_ACKED;
				}
			}
			break;
		}
		case MSG_NACK: {
			if (Serial.available() >= 1) {
				lastNackedMsgID = (serialMsgID)Serial.read(); // mark if message is misunderstood
				if ( lastNackedMsgID < MSG_COUNT){
					txState[lastNackedMsgID].status = TX_NACKED;
				}
			}
			break;
		}
		case MSG_ERROR: {
			if (Serial.available() >= 1) {
				lastErrorCode = (errCode)Serial.read(); // error code for logging
			}
			break;
		}
		default: {
			// Unknown message rxID — send NACK
			txNack(rxID);
			break;
		}
	}
	return true;
}

// ************** TX Messages **************
txStatus txRaceMode(raceMode newMode) {
	auto& state 			= txState[MSG_RACE_MODE];
	unsigned long now 		= millis();
	uint8_t payload 		= (uint8_t)newMode;			// set payload
	switch (state.status) {
		case TX_SENT:
			if (now - state.sendTime >= txTimeout){		// check if response waiting exceeded
				return state.status		= TX_TIMEOUT;
			}
		case TX_ACKED:
		case TX_FAILED:
		case TX_TIMEOUT:
			return state.status;
		case TX_NONE:	
		case TX_NACKED:
			if (state.retries > maxRetries){			// check if retries exceeded
				return state.status = TX_FAILED;
			}
			sendMessage(MSG_RACE_MODE, &payload, 1);	// send payload	
			state.sendTime 	= now;						// timestamp transmission
			state.retries++;							// increment retries
			return state.status 	= TX_SENT;
	}
}

txStatus txRaceState(raceState newState){
	auto& state 			= txState[MSG_RACE_STATE];
	unsigned long now 		= millis();
	uint8_t payload 		= (uint8_t)newState;		// set payload
	switch (state.status) {
		case TX_SENT:
			if (now - state.sendTime >= txTimeout){		// check if response waiting exceeded
				return state.status 		= TX_TIMEOUT;
			}
		case TX_ACKED:
		case TX_FAILED:
		case TX_TIMEOUT:
			return state.status;
		case TX_NONE:	
		case TX_NACKED:
			if (state.retries > maxRetries){			// check if retries exceeded
				return state.status	= TX_FAILED;
			}
			sendMessage(MSG_RACE_STATE, &payload, 1);	// send payload	
			state.sendTime 	= now;						// timestamp transmission
			state.retries++;							// increment retries
			return state.status 	= TX_SENT;
	}
}

txStatus txCarID(uint8_t* uid, bool isLeft){
	serialMsgID msgID;
	if (isLeft){
		msgID 				= MSG_LEFT_CAR_ID;			// set message ID
	} else {
		msgID 				= MSG_RIGHT_CAR_ID;			// set message ID
	}
	auto& state 			= txState[msgID];
	unsigned long now 		= millis();
	switch (state.status) {
		case TX_SENT:
			if (now - state.sendTime >= txTimeout){		// check if response waiting exceeded
				return state.status 		= TX_TIMEOUT;
			}
		case TX_ACKED:
		case TX_FAILED:
		case TX_TIMEOUT:
			return state.status;
		case TX_NONE:	
		case TX_NACKED:
			if (state.retries > maxRetries){			// check if retries exceeded
				return state.status = TX_FAILED;
			}
			sendMessage(msgID, uid, serialUIDLength);	// send payload	
			state.sendTime	= now;						// timestamp transmission
			state.retries++;							// increment retries
			return state.status 	= TX_SENT;
	}
}

txStatus txRaceStart(uint8_t start){
	auto& state = txState[MSG_RACE_START];
	unsigned long now 	= millis();
	uint8_t payload 	= start; 						// race=0b0001, left=0b0010, right=0b0100
	switch (state.status) {
		case TX_SENT:
			if (now - state.sendTime >= txTimeout){		// check if response waiting exceeded
				return state.status 	= TX_TIMEOUT;
			}
		case TX_ACKED:
		case TX_FAILED:
		case TX_TIMEOUT:
			return state.status;
		case TX_NONE:	
		case TX_NACKED:
			if (state.retries > maxRetries){			// check if retries exceeded
				return state.status = TX_FAILED;
			}
			sendMessage(MSG_RACE_START, &payload, 1);	//send payload	
			state.sendTime 	= now;						// timestamp transmission
			state.retries++;							// increment retries
			return state.status 	= TX_SENT;
	}
}

txStatus txReactionTime(uint32_t reactionTime, bool isLeft){
	serialMsgID msgID;
	if (isLeft){
		msgID 				= MSG_LEFT_REACT;			// set message ID
	} else {
		msgID 				= MSG_RIGHT_REACT;			// set message ID
	}
	auto& state 			= txState[msgID];
	unsigned long now 		= millis();
	uint8_t payload[sizeof(uint32_t)];					// set payload
	memcpy(payload, &reactionTime, sizeof(uint32_t));
	switch (state.status) {
		case TX_SENT:
			if (now - state.sendTime >= txTimeout){		// check if response waiting exceeded
				return state.status 	= TX_TIMEOUT;
			}
		case TX_ACKED:
		case TX_FAILED:
		case TX_TIMEOUT:
			return state.status;
		case TX_NONE:	
		case TX_NACKED:
			if (state.retries > maxRetries){			// check if retries exceeded
				return state.status = TX_FAILED;
			}
			sendMessage(msgID, &payload, sizeof(payload));// send payload	
			state.sendTime 	= now;						// timestamp transmission
			state.retries++;							// increment retries
			return state.status 	= TX_SENT;
	}
}

txStatus txFoulStatus(uint8_t foul){
	auto& state 			= txState[MSG_FOUL];
	unsigned long now 		= millis();
	uint8_t payload 		= (uint8_t)foul; 			// left=0b0001, right=0b0010
	switch (state.status) {
		case TX_SENT:
			if (now - state.sendTime >= txTimeout){		// check if response waiting exceeded
				return state.status 	= TX_TIMEOUT;
			}
		case TX_ACKED:
		case TX_FAILED:
		case TX_TIMEOUT:
			return state.status;
		case TX_NONE:	
		case TX_NACKED:
			if (state.retries > maxRetries){			// check if retries exceeded
				return state.status = TX_FAILED;
			}
			sendMessage(MSG_FOUL, &payload, 1);// send payload	
			state.sendTime 	= now;						// timestamp transmission
			state.retries++;							// increment retries
			return state.status 	= TX_SENT;
	}

}

txStatus txWinner(uint8_t winner){
	auto& state 			= txState[MSG_WINNER];
	unsigned long now 		= millis();
	uint8_t payload 		= (uint8_t)winner; 			// tie=0b0000, left=0b0001, right=0b0010
	switch (state.status) {
		case TX_SENT:
			if (now - state.sendTime >= txTimeout){		// check if response waiting exceeded
				return state.status 	= TX_TIMEOUT;
			}
		case TX_ACKED:
		case TX_FAILED:
		case TX_TIMEOUT:
			return state.status;
		case TX_NONE:	
		case TX_NACKED:
			if (state.retries > maxRetries){			// check if retries exceeded
				return state.status = TX_FAILED;
			}
			sendMessage(MSG_WINNER, &payload, sizeof(payload));// send payload	
			state.sendTime 	= now;						// timestamp transmission
			state.retries++;							// increment retries
			return state.status 	= TX_SENT;
	}
}

txStatus txDisplayAdvance(){
	auto& state 			= txState[MSG_DISP_ADVANCE];
	unsigned long now 		= millis();
	switch (state.status) {
		case TX_SENT:
			if (now - state.sendTime >= txTimeout){		// check if response waiting exceeded
				return state.status 	= TX_TIMEOUT;
			}
		case TX_ACKED:
		case TX_FAILED:
		case TX_TIMEOUT:
			return state.status;
		case TX_NONE:	
		case TX_NACKED:
			if (state.retries > maxRetries){			// check if retries exceeded
				return state.status = TX_FAILED;
			}
			sendMessage(MSG_DISP_ADVANCE, nullptr, 0);// send payload	
			state.sendTime 	= now;						// timestamp transmission
			state.retries++;							// increment retries
			return state.status 	= TX_SENT;
	}
}

txStatus txError(errCode err){
	auto& state 			= txState[MSG_ERROR];
	unsigned long now 		= millis();
	uint8_t payload 		= (uint8_t)err;
	switch (state.status) {
		case TX_SENT:
			if (now - state.sendTime >= txTimeout){		// check if response waiting exceeded
				return state.status 	= TX_TIMEOUT;
			}
		case TX_ACKED:
		case TX_FAILED:
		case TX_TIMEOUT:
			return state.status;
		case TX_NONE:	
		case TX_NACKED:
			if (state.retries > maxRetries){			// check if retries exceeded
				return state.status = TX_FAILED;
			}
			sendMessage(MSG_ERROR, &payload, 1);		// send payload	
			state.sendTime 	= now;						// timestamp transmission
			state.retries++;							// increment retries
			return state.status 	= TX_SENT;
	}
}

void txAck(uint8_t ackID){
	Serial.write((uint8_t)MSG_ACK);
	Serial.write(ackID);
}

void txNack(uint8_t nackID){
	Serial.write((uint8_t)MSG_NACK);
	Serial.write(nackID);
}

// ************** Helper Functions **************
void sendMessage(serialMsgID id, const uint8_t* data, uint8_t dataLen) {
    Serial.write((uint8_t)id);
    Serial.write(data, dataLen);
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

		case MSG_LEFT_CAR_ID:
		case MSG_RIGHT_CAR_ID:
			return serialUIDLength;  // raw UID

		case MSG_LEFT_REACT:
		case MSG_RIGHT_REACT:
			return sizeof(uint32_t);

		case MSG_DISP_ADVANCE:
			return 0;

		default:
			return 0;  // unknown = no payload
	}
}

void resetTxState(serialMsgID id) {
  if (id < MSG_COUNT) {
    txState[id] = TxTracker{TX_NONE, 0, 0};
  }
}