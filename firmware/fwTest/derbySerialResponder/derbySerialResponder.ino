/*
 * DerbyTimer Serial Protocol Responder
 * =====================================
 * Target board: Arduino Nano (ATmega328P)
 *
 * Wiring:
 *   Responder D0 (RX) <-------- Tester D5 (TX)
 *   Responder D1 (TX) --------> Tester D6 (RX)
 *   GND                         GND
 *
 * Compile:
 *   arduino-cli compile --fqbn arduino:avr:nano firmware/fwTest/derbySerialResponder/derbySerialResponder.ino
 *
 * Default behavior: ACK every valid message received.
 *
 * Mode control: The Tester sends byte 0xFD followed by a mode byte to
 * change the Responder's behavior for the next received message:
 *   0x00 = ACK all (default, always safe to restore)
 *   0x01 = NACK next message, then revert to ACK_ALL
 *   0x02 = NACK all messages until mode is explicitly changed
 *   0x03 = No response to next message (silent), then revert to ACK_ALL
 *
 * Note: This sketch does NOT use serialComm.cpp. It implements the protocol
 * byte-by-byte for full control over ACK/NACK behavior during testing.
 */

#include <Arduino.h>

// ==================== PROTOCOL CONSTANTS ====================
// The responder intentionally does NOT include serialComm.h -- it is a
// bare-metal parser so it can ACK, NACK, or stay silent independently of
// the production code. Keep the two tables below in sync manually:
//   ID values  -> serialMsgID enum in serialComm.h
//   payloadLen -> getExpectedPayloadLength() in serialComm.cpp
enum MsgID : uint8_t {
    ID_NULL,         // 0
    ID_ACK,          // 1
    ID_NACK,         // 2
    ID_RACE_MODE,    // 3
    ID_RACE_STATE,   // 4
    ID_RACE_START,   // 5
    ID_ERROR,        // 6
    ID_LEFT_REACT,   // 7
    ID_RIGHT_REACT,  // 8
    ID_FOUL,         // 9
    ID_WINNER,       // 10
    ID_DISP_ADVANCE, // 11
    ID_COUNT         // 12
};

static uint8_t payloadLen(uint8_t id) {
    switch ((MsgID)id) {
        case ID_RACE_MODE: case ID_RACE_STATE:
        case ID_ERROR: case ID_FOUL: case ID_WINNER: case ID_ACK: case ID_NACK:
            return 1;
        case ID_LEFT_REACT: case ID_RIGHT_REACT:
            return 4;
        default:   // ID_RACE_START and ID_DISP_ADVANCE are zero-payload
            return 0;
    }
}

// ==================== RESPONDER MODE ====================
#define CTRL_BYTE   0xFD
enum RespMode : uint8_t { RESP_ACK_ALL = 0, RESP_NACK_ONCE, RESP_NACK_ALL, RESP_SILENT };
static RespMode respMode = RESP_ACK_ALL;

// ==================== PARSER STATE ====================
static bool    ctrlPending = false;
static uint8_t msgID       = 0;
static uint8_t msgLen      = 0;
static uint8_t msgPos      = 0;  // 0 = waiting for ID; 1..N = collecting payload bytes

// ==================== RESPONSE LOGIC ====================
static void sendAck(uint8_t id) {
    Serial.write((uint8_t)ID_ACK);
    Serial.write(id);
}

static void sendNack(uint8_t id) {
    Serial.write((uint8_t)ID_NACK);
    Serial.write(id);
}

static void handleMessage() {
    if (msgID == (uint8_t)ID_NULL) return;  // ignore null messages silently

    switch (respMode) {
        case RESP_ACK_ALL:
            sendAck(msgID);
            break;
        case RESP_NACK_ONCE:
            sendNack(msgID);
            respMode = RESP_ACK_ALL;
            break;
        case RESP_NACK_ALL:
            sendNack(msgID);
            break;
        case RESP_SILENT:
            respMode = RESP_ACK_ALL;
            break;
    }
}

// ==================== SETUP & LOOP ====================

void setup() {
    Serial.begin(115200);
    delay(1000);
}

void loop() {
    while (Serial.available()) {
        uint8_t b = Serial.read();

        // Control byte 0xFD resets parser and starts mode-change sequence
        if (b == CTRL_BYTE) {
            ctrlPending = true;
            msgPos = 0;
            continue;
        }
        if (ctrlPending) {
            respMode    = (RespMode)(b & 0x03);
            ctrlPending = false;
            continue;
        }

        if (msgPos == 0) {
            // Waiting for message ID byte
            if (b >= ID_COUNT) continue;
            msgID  = b;
            msgLen = payloadLen(b);
            msgPos = 1;
            if (msgLen == 0) {
                msgPos = 0;
                handleMessage();
            }
        } else {
            // Collecting payload bytes (msgPos tracks which byte we are on, 1-indexed)
            if (msgPos >= msgLen) {
                // Last payload byte
                msgPos = 0;
                handleMessage();
            } else {
                msgPos++;
            }
        }
    }
}
