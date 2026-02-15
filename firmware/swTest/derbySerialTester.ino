/*
 * DerbyTimer Serial Protocol Test Harness
 * ========================================
 * 
 * Purpose:	Comprehensive UART protocol testing for DerbyTimer project
 * Target:	startController (Nano)
 * DUT:  	finishController (Nano 33 BLE)
 * 
 * Wiring:
 *   Tester TX (Pin 5) --> DUT RX
 *   Tester RX (Pin 6) --> DUT TX
 *   GND               --> GND
 * 
 * Usage:
 *   1. Upload to tester Arduino
 *   2. Connect wiring to DUT
 *   3. Open Serial Monitor at 115200 baud
 *   4. Send commands via Serial Monitor:
 *      - 'a' = Run all tests
 *      - '1' = Test MSG_RACE_MODE
 *      - '2' = Test MSG_RACE_STATE
 *      - '3' = Test MSG_RACE_START
 *      - '4' = Test MSG_LEFT_REACT
 *      - '5' = Test MSG_RIGHT_REACT
 *      - '6' = Test MSG_FOUL
 *      - '7' = Test MSG_WINNER
 *      - '8' = Test MSG_DISP_ADVANCE
 *      - '9' = Test MSG_ERROR
 *      - 't' = Timing stress test
 *      - 'e' = Error handling test
 *      - 'r' = Reset DUT state
 *      - 's' = Sequence test (full race simulation)
 *      - 'h' = Help
 */
 
#include <Arduino.h>
#include "serialComm.h"
#include <SoftwareSerial.h>
// ==================== PIN CONFIGURATION ====================

#define DEBUG_TX_PIN 5
#define DEBUG_RX_PIN 6
SoftwareSerial debug(DEBUG_RX_PIN, DEBUG_TX_PIN);
// Hardware Serial (D0/D1) to BLE board

// ==================== PROTOCOL DEFINITIONS ====================

enum raceState : uint8_t { 
    RACE_IDLE,
    RACE_STAGING,
    RACE_COUNTDOWN,
    RACE_RACING, 
    RACE_COMPLETE,
    RACE_TEST
};

enum raceMode : uint8_t {
    MODE_GATEDROP,
    MODE_REACTION,
    MODE_PRO,
    MODE_DIALIIN 
};

// Bitmask definitions
#define winner_leftWin   0b0001
#define winner_rightWin  0b0010
#define winner_tie       0b0100
#define start_race       0b0001
#define start_left       0b0010
#define start_right      0b0100
#define foul_left        0b0001
#define foul_right       0b0010

// ==================== TEST CONFIGURATION ====================

#define BAUD_RATE       115200
#define ACK_TIMEOUT_MS  100      // Time to wait for ACK (protocol uses 50ms)
#define TEST_DELAY_MS   50       // Delay between test messages
#define RX_BUFFER_SIZE  32

// ==================== TEST STATISTICS ====================

struct TestStats {
    uint16_t totalTests;
    uint16_t passed;
    uint16_t failed;
    uint16_t timeouts;
    uint32_t minResponseUs;
    uint32_t maxResponseUs;
    uint32_t totalResponseUs;
    uint16_t responseCount;
} stats;

// ==================== RX STATE ====================

uint8_t rxBuffer[RX_BUFFER_SIZE];
uint8_t rxIndex = 0;
serialMsgID lastRxID = MSG_NULL;
uint8_t lastRxPayload[8];
uint8_t lastRxPayloadLen = 0;
bool messageReceived = false;

// ==================== HELPER FUNCTIONS ====================

const char* getMsgName(serialMsgID id) {
    switch(id) {
        case MSG_NULL:        return "MSG_NULL";
        case MSG_ACK:         return "MSG_ACK";
        case MSG_NACK:        return "MSG_NACK";
        case MSG_RACE_MODE:   return "MSG_RACE_MODE";
        case MSG_RACE_STATE:  return "MSG_RACE_STATE";
        case MSG_RACE_START:  return "MSG_RACE_START";
        case MSG_ERROR:       return "MSG_ERROR";
        case MSG_LEFT_REACT:  return "MSG_LEFT_REACT";
        case MSG_RIGHT_REACT: return "MSG_RIGHT_REACT";
        case MSG_LEFT_RESULT: return "MSG_LEFT_RESULT";
        case MSG_RIGHT_RESULT:return "MSG_RIGHT_RESULT";
        case MSG_FOUL:        return "MSG_FOUL";
        case MSG_WINNER:      return "MSG_WINNER";
        case MSG_DISP_ADVANCE:return "MSG_DISP_ADVANCE";
        default:              return "UNKNOWN";
    }
}

const char* getStateName(raceState state) {
    switch(state) {
        case RACE_IDLE:      return "IDLE";
        case RACE_STAGING:   return "STAGING";
        case RACE_COUNTDOWN: return "COUNTDOWN";
        case RACE_RACING:    return "RACING";
        case RACE_COMPLETE:  return "COMPLETE";
        case RACE_TEST:      return "TEST";
        default:             return "UNKNOWN";
    }
}

const char* getModeName(raceMode mode) {
    switch(mode) {
        case MODE_GATEDROP:  return "GATEDROP";
        case MODE_REACTION:  return "REACTION";
        case MODE_PRO:       return "PRO";
        case MODE_DIALIIN:   return "DIALIN";
        default:             return "UNKNOWN";
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

void clearRxBuffer() {
    while (dutSerial.available()) {
        dutSerial.read();
    }
    rxIndex = 0;
    messageReceived = false;
    lastRxID = MSG_NULL;
    lastRxPayloadLen = 0;
}

void resetStats() {
    memset(&stats, 0, sizeof(stats));
    stats.minResponseUs = 0xFFFFFFFF;
}

void printStats() {
    debug.println(F("\n========== TEST SUMMARY =========="));
    debug.print(F("Total Tests:  ")); debug.println(stats.totalTests);
    debug.print(F("Passed:       ")); debug.println(stats.passed);
    debug.print(F("Failed:       ")); debug.println(stats.failed);
    debug.print(F("Timeouts:     ")); debug.println(stats.timeouts);
    
    if (stats.responseCount > 0) {
        debug.println(F("\nResponse Time Statistics:"));
        debug.print(F("  Min:  ")); debug.print(stats.minResponseUs); debug.println(F(" us"));
        debug.print(F("  Max:  ")); debug.print(stats.maxResponseUs); debug.println(F(" us"));
        debug.print(F("  Avg:  ")); 
        debug.print(stats.totalResponseUs / stats.responseCount); 
        debug.println(F(" us"));
    }
    
    float passRate = stats.totalTests > 0 ? 
        (float)stats.passed / stats.totalTests * 100.0 : 0.0;
    debug.print(F("\nPass Rate: ")); 
    debug.print(passRate, 1); 
    debug.println(F("%"));
    debug.println(F("==================================\n"));
}


// ==================== TX FUNCTIONS ====================

void sendMessage(serialMsgID id, const uint8_t* data, uint8_t dataLen) {
    dutSerial.write((uint8_t)id);
    if (data != nullptr && dataLen > 0) {
        dutSerial.write(data, dataLen);
    }
    
    // Debug output
    debug.print(F("  TX: ")); 
    debug.print(getMsgName(id));
    if (dataLen > 0) {
        debug.print(F(" ["));
        for (uint8_t i = 0; i < dataLen; i++) {
            if (i > 0) debug.print(F(" "));
            debug.print(data[i], HEX);
        }
        debug.print(F("]"));
    }
    debug.println();
}

void sendAck(serialMsgID idToAck) {
    uint8_t payload = (uint8_t)idToAck;
    sendMessage(MSG_ACK, &payload, 1);
}

void sendNack(serialMsgID idToNack) {
    uint8_t payload = (uint8_t)idToNack;
    sendMessage(MSG_NACK, &payload, 1);
}

// ==================== RX FUNCTIONS ====================

bool processRx() {
    while (dutSerial.available()) {
        uint8_t b = dutSerial.read();
        
        if (rxIndex == 0) {
            // First byte is message ID
            if (b >= MSG_COUNT) {
                // Invalid ID, skip
                continue;
            }
            lastRxID = (serialMsgID)b;
            lastRxPayloadLen = getExpectedPayloadLength(lastRxID);
            rxBuffer[rxIndex++] = b;
        } else {
            rxBuffer[rxIndex++] = b;
        }
        
        // Check if complete message received
        if (rxIndex >= 1 + lastRxPayloadLen) {
            // Copy payload
            for (uint8_t i = 0; i < lastRxPayloadLen; i++) {
                lastRxPayload[i] = rxBuffer[1 + i];
            }
            messageReceived = true;
            rxIndex = 0;
            return true;
        }
    }
    return false;
}

bool waitForResponse(unsigned long timeoutMs) {
    unsigned long startTime = millis();
    unsigned long startUs = micros();
    
    while ((millis() - startTime) < timeoutMs) {
        if (processRx()) {
            unsigned long responseUs = micros() - startUs;
            
            // Update stats
            stats.totalResponseUs += responseUs;
            stats.responseCount++;
            if (responseUs < stats.minResponseUs) stats.minResponseUs = responseUs;
            if (responseUs > stats.maxResponseUs) stats.maxResponseUs = responseUs;
            
            // Debug output
            debug.print(F("  RX: "));
            debug.print(getMsgName(lastRxID));
            if (lastRxPayloadLen > 0) {
                debug.print(F(" ["));
                for (uint8_t i = 0; i < lastRxPayloadLen; i++) {
                    if (i > 0) debug.print(F(" "));
                    debug.print(lastRxPayload[i], HEX);
                }
                Serial.print(F("]"));
            }
            debug.print(F(" (")); 
            debug.print(responseUs); 
            debug.println(F(" us)"));
            return true;
        }
    }
    return false;
}


// ==================== TEST FUNCTIONS ====================

bool testExpectAck(serialMsgID sentID) {
    stats.totalTests++;
    
    if (!waitForResponse(ACK_TIMEOUT_MS)) {
        debug.println(F("  FAIL: Timeout waiting for ACK"));
        stats.timeouts++;
        stats.failed++;
        return false;
    }
    
    if (lastRxID != MSG_ACK) {
        debug.print(F("  FAIL: Expected ACK, got "));
        debug.println(getMsgName(lastRxID));
        stats.failed++;
        return false;
    }
    
    if (lastRxPayload[0] != sentID) {
        debug.print(F("  FAIL: ACK for wrong message. Expected "));
        debug.print(getMsgName(sentID));
        debug.print(F(", got ACK for "));
        debug.println(lastRxPayload[0]);
        stats.failed++;
        return false;
    }
    
    debug.println(F("  PASS"));
    stats.passed++;
    return true;
}

// ==================== INDIVIDUAL MESSAGE TESTS ====================



// ==================== ADVANCED TESTS ====================



// ==================== MAIN TEST RUNNERS ====================

void runAllTests() {
    debug.println(F("\n╔════════════════════════════════════════╗"));
    debug.println(F("║     DERBYTIMER PROTOCOL TEST SUITE     ║"));
    debug.println(F("╚════════════════════════════════════════╝"));
    resetStats();   
    resetDUT();
    delay(200);
    testRaceMode();			// Test #1
    delay(200);
    testRaceState();		// Test #2
    delay(200);
    testRaceStart();		// Test #3
    delay(200);
    testReactionTime();		// Test #4
    delay(200);
    testFoul();				// Test #5
    delay(200);
    testWinner();			// Test #6
    delay(200);
    testDisplayAdvance();	// Test #7
    delay(200);
    testError();			// Test #8
    delay(200);
    printStats();
}

void printHelp() {
    debug.println(F("\n=== DerbyTimer Comm Protocol Tester Commands ==="));
    debug.println(F("a - Run ALL tests"));
    debug.println(F("1 - Test MSG_RACE_MODE"));
    debug.println(F("2 - Test MSG_RACE_STATE"));
    debug.println(F("3 - Test MSG_RACE_START"));
    debug.println(F("4 - Test MSG_LEFT_REACT"));
    debug.println(F("5 - Test MSG_RIGHT_REACT"));
    debug.println(F("6 - Test MSG_FOUL"));
    debug.println(F("7 - Test MSG_WINNER"));
    debug.println(F("8 - Test MSG_DISP_ADVANCE"));
    debug.println(F("9 - Test MSG_ERROR"));
    debug.println(F("t - Timing stress test"));
    debug.println(F("e - Error handling test"));
    debug.println(F("r - Reset DUT state"));
    debug.println(F("s - Full race sequence test"));
    debug.println(F("p - Print current stats"));
    debug.println(F("c - Clear stats"));
    debug.println(F("h - Show this help"));
    debug.println();
}


// ==================== SETUP & LOOP ====================

void setup() {
	# define dutSerial Serial
	dutSerial.begin(BAUD_RATE);
	debug.begin(BAUD_RATE);
	
	delay(1000);
    
    debug.println(F("\n╔════════════════════════════════════════╗"));
    debug.println(F("║   DerbyTimer Serial Protocol Tester    ║"));
    debug.println(F("║        v1.0 - UART Test Harness        ║"));
    debug.println(F("╚════════════════════════════════════════╝"));
    debug.println();
    debug.println(F("Wiring:"));
    debug.print(F("  Tester TX (Pin ")); debug.print(DEBUG_TX_PIN); debug.println(F(") --> Terminal RX"));
    debug.print(F("  Tester RX (Pin ")); debug.print(DEBUG_RX_PIN); debug.println(F(") --> Terminal TX"));
    debug.println(F("  GND --> GND"));
    debug.println();
    debug.println(F("Press 'h' for help, 'a' to run all tests"));
    
    resetStats();
}

void loop() {
	
	if (debug.available()){
		char cmd = debug.read();
		
		switch (cmd) {
			case 'a': case 'A':
				runAllTests();
				break;
			case '1':
				resetStats();
				testRaceMode();
				printStats();
				break;
			case '2':
				resetStats();
				testRaceState();
				printStats();
				break;
			case '3':
				resetStats();
				testRaceStart();
				printStats();
				break;
			case '4':
				resetStats();
				testReactionTime();
				printStats();
				break;
			case '5':
				resetStats();
				testFoul();
				printStats();
				break;
			case '6':
				resetStats();
				testWinner();
				printStats();
				break;
			case '7':
				resetStats();
				testDisplayAdvance();
				printStats();
				break;
			case '8':
				resetStats();
				testError();
				printStats();
				break;
			case 't': case: 'T':
				resetStats();
				testTimingStress();
				printStats();
				break;
			case 'e': case: 'E':
				resetStats();
				testErrorHandling();
				printStats();
				break;
			case 'r': case: 'R':
				resetStats();
				resetDUT();
				printStats();
				break;
			case 's': case: 'S':
				resetStats();
				testFullRaceSequence();
				printStats();
				break;
			case 'p': case: 'P':
				printStats();
				break;
			case 'c': case: 'C':
				resetStats();
				debug.println(F("Stats cleared."));
				break;
			case 'h': case: 'H':
				printHelp();
				break;
			case '/n': case: '/r':
				// Ignore new lines
				break;
			default:
				debug.pint(F("Unknown command: '"));
				debug.print(cmd);
				debug.println(F("'. Press 'h' for help."));
				break;
		} // End of switch (cmd)
	} // End of if(debug.available)
		
	// Process unsolicited messages from DUT
	if (processRx()) {
		debug.print(F("[Unsolicited] "));
		debug.print(getMsgName(lastRxID));
		if (lastRxPayloadLen > 0) {
			debug.print(F(" ["));
			for (uint8_t i = 0; i < lastRxPayloadLen; i++) {
				if (i > 0) debug.print(F(" "));
				debug.print(lastRxPayload[i],HEX);
			}
			debug.print(F("]"));
		}
		debug.println();
	}

} // end of void loop()