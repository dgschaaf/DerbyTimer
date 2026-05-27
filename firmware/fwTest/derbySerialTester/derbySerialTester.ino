/*
 * DerbyTimer Serial Protocol Test Harness
 * ========================================
 * Target board: Arduino Nano (ATmega328P)
 *
 * Wiring:
 *   Tester D5 (TX) --------> Responder D0 (RX)
 *   Tester D6 (RX) <-------- Responder D1 (TX)
 *   GND                      GND
 *   Tester USB --> PC serial monitor (115200 baud)
 *
 * Compile:
 *   arduino-cli compile --fqbn arduino:avr:nano --library firmware/lib/shared firmware/fwTest/derbySerialTester/derbySerialTester.ino
 *
 * Commands (serial monitor at 115200):
 *   a - Run all tests
 *   1..9  - Individual message tests
 *   t - Timing stress (50 messages)
 *   e - Error-handling tests (NACK / timeout)
 *   r - Reset DUT state
 *   s - Full race sequence
 *   p - Print stats
 *   h - Help
 */

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <serialComm.h>
#include <raceTypes.h>

// ==================== RESPONDER CONTROL ====================
// Tester sends 0xFD + mode byte to configure Responder behavior.
#define RESP_CTRL       0xFD
#define RESP_ACK_ALL    0x00  // default: ACK everything
#define RESP_NACK_ONCE  0x01  // NACK next msg, then revert to ACK_ALL
#define RESP_NACK_ALL   0x02  // NACK everything until changed
#define RESP_SILENT     0x03  // drop next msg (no response), then revert

// ==================== SERIAL PORTS ====================
#define DUT_TX_PIN 5
#define DUT_RX_PIN 6
// SoftwareSerial at 115200 on 16 MHz AVR may show timing jitter over long cables.
// If instability is observed, evaluate lower baud per P2-36.
SoftwareSerial dut(DUT_RX_PIN, DUT_TX_PIN);

#define BAUD_RATE      115200
#define ACK_TIMEOUT_MS 200   // 50ms tx timeout * 3 retries + margin
#define TEST_DELAY_MS   50

// ==================== TEST STATISTICS ====================
struct TestStats {
    uint16_t total;
    uint16_t passed;
    uint16_t failed;
    uint16_t timeouts;
    uint32_t minUs;
    uint32_t maxUs;
    uint32_t totalUs;
    uint16_t count;
} stats;

void resetStats() {
    memset(&stats, 0, sizeof(stats));
    stats.minUs = 0xFFFFFFFFUL;
}

void printStats() {
    Serial.println(F("\n========== TEST SUMMARY =========="));
    Serial.print(F("Total: "));    Serial.print(stats.total);
    Serial.print(F("  Pass: "));   Serial.print(stats.passed);
    Serial.print(F("  Fail: "));   Serial.print(stats.failed);
    Serial.print(F("  Timeout: ")); Serial.println(stats.timeouts);
    if (stats.count > 0) {
        Serial.print(F("Round-trip us  min: ")); Serial.print(stats.minUs);
        Serial.print(F("  max: "));              Serial.print(stats.maxUs);
        Serial.print(F("  avg: "));
        Serial.println(stats.totalUs / stats.count);
    }
    Serial.println(F("=================================="));
}

// ==================== RESPONDER MODE CONTROL ====================
void setRespMode(uint8_t mode) {
    dut.write((uint8_t)RESP_CTRL);
    dut.write(mode);
    delay(20);
}

// ==================== LOW-LEVEL TX ====================
void dutSend(uint8_t id, const uint8_t* data, uint8_t len) {
    dut.write(id);
    if (data && len > 0) dut.write(data, len);
    Serial.print(F("  TX: 0x")); Serial.print(id, HEX);
    if (len > 0 && data) {
        Serial.print(F(" ["));
        for (uint8_t i = 0; i < len; i++) {
            if (i) Serial.print(' ');
            Serial.print(data[i], HEX);
        }
        Serial.print(']');
    }
    Serial.println();
}

// ==================== LOW-LEVEL RX ====================
static uint8_t rxBuf[16];
static uint8_t rxPos      = 0;
static uint8_t lastRxID   = 0;
static uint8_t lastRxPay[8] = {};
static uint8_t lastRxLen  = 0;

bool parseDut() {
    while (dut.available()) {
        uint8_t b = dut.read();
        if (rxPos == 0) {
            if (b >= MSG_COUNT) continue;
            lastRxID  = b;
            lastRxLen = getExpectedPayloadLength((serialMsgID)b);
            rxPos = 1;
            if (lastRxLen == 0) { rxPos = 0; return true; }
        } else {
            rxBuf[rxPos - 1] = b;
            if (rxPos >= lastRxLen) {
                memcpy(lastRxPay, rxBuf, lastRxLen);
                rxPos = 0;
                return true;
            }
            rxPos++;
        }
    }
    return false;
}

bool waitResponse(uint16_t timeoutMs) {
    uint32_t start   = millis();
    uint32_t startUs = micros();
    while ((millis() - start) < timeoutMs) {
        if (parseDut()) {
            uint32_t rtt = micros() - startUs;
            stats.totalUs += rtt;
            stats.count++;
            if (rtt < stats.minUs) stats.minUs = rtt;
            if (rtt > stats.maxUs) stats.maxUs = rtt;
            Serial.print(F("  RX: 0x")); Serial.print(lastRxID, HEX);
            if (lastRxLen > 0) {
                Serial.print(F(" ["));
                for (uint8_t i = 0; i < lastRxLen; i++) {
                    if (i) Serial.print(' ');
                    Serial.print(lastRxPay[i], HEX);
                }
                Serial.print(']');
            }
            Serial.print(F(" (")); Serial.print(rtt); Serial.println(F(" us)"));
            return true;
        }
    }
    return false;
}

// ==================== TEST HELPERS ====================
bool passTest(const __FlashStringHelper* msg) {
    stats.total++; stats.passed++;
    Serial.print(F("  PASS: ")); Serial.println(msg);
    return true;
}

bool failTest(const __FlashStringHelper* msg) {
    stats.total++; stats.failed++;
    Serial.print(F("  FAIL: ")); Serial.println(msg);
    return false;
}

bool timeoutTest(const __FlashStringHelper* msg) {
    stats.total++; stats.timeouts++; stats.failed++;
    Serial.print(F("  TIMEOUT: ")); Serial.println(msg);
    return false;
}

// Send id+data, wait for ACK with matching echo byte.
bool expectAck(uint8_t id, const uint8_t* data, uint8_t len) {
    dutSend(id, data, len);
    if (!waitResponse(ACK_TIMEOUT_MS)) return timeoutTest(F("no ACK"));
    if (lastRxID != MSG_ACK)           return failTest(F("expected ACK"));
    if (lastRxPay[0] != id)            return failTest(F("ACK wrong ID"));
    return passTest(F("ACK ok"));
}

// ==================== TEST FUNCTIONS ====================

void testRaceMode() {
    Serial.println(F("\n[TEST] MSG_RACE_MODE (all 4 modes)"));
    for (uint8_t m = 0; m < MODE_COUNT; m++) {
        expectAck(MSG_RACE_MODE, &m, 1);
        delay(TEST_DELAY_MS);
    }
}

void testRaceState() {
    Serial.println(F("\n[TEST] MSG_RACE_STATE (all 6 states)"));
    for (uint8_t s = 0; s <= RACE_TEST; s++) {
        expectAck(MSG_RACE_STATE, &s, 1);
        delay(TEST_DELAY_MS);
    }
}

void testRaceStart() {
    Serial.println(F("\n[TEST] MSG_RACE_START (priority message)"));
    uint8_t mask = start_all;
    expectAck(MSG_RACE_START, &mask, 1);
}

void testReactionTime() {
    Serial.println(F("\n[TEST] MSG_LEFT_REACT / MSG_RIGHT_REACT (4-byte round-trip)"));
    int32_t vals[] = { 0, 12345, -5000, 0x7FFFFFFF };
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t buf[4];
        memcpy(buf, &vals[i], 4);
        expectAck(MSG_LEFT_REACT,  buf, 4);
        expectAck(MSG_RIGHT_REACT, buf, 4);
        delay(TEST_DELAY_MS);
    }
}

void testFoul() {
    Serial.println(F("\n[TEST] MSG_FOUL (bitmask round-trip)"));
    uint8_t masks[] = { foul_left, foul_right, foul_both };
    for (uint8_t i = 0; i < 3; i++) {
        expectAck(MSG_FOUL, &masks[i], 1);
        delay(TEST_DELAY_MS);
    }
}

void testWinner() {
    Serial.println(F("\n[TEST] MSG_WINNER (all 4 outcomes)"));
    uint8_t masks[] = { winner_leftWin, winner_rightWin, winner_tie, winner_noResult };
    for (uint8_t i = 0; i < 4; i++) {
        expectAck(MSG_WINNER, &masks[i], 1);
        delay(TEST_DELAY_MS);
    }
}

void testDisplayAdvance() {
    Serial.println(F("\n[TEST] MSG_DISP_ADVANCE (zero payload)"));
    expectAck(MSG_DISP_ADVANCE, nullptr, 0);
}

void testError() {
    Serial.println(F("\n[TEST] MSG_ERROR (ACK expected -- verifies P2-15 fix)"));
    for (uint8_t e = 1; e < err_Count; e++) {
        expectAck(MSG_ERROR, &e, 1);
        delay(TEST_DELAY_MS);
    }
}

void testNackRetry() {
    Serial.println(F("\n[TEST] NACK retry -- Responder NACKs first, ACKs second"));
    setRespMode(RESP_NACK_ONCE);
    uint8_t payload = MODE_GATEDROP;
    dutSend(MSG_RACE_MODE, &payload, 1);

    if (!waitResponse(ACK_TIMEOUT_MS)) { timeoutTest(F("no NACK on 1st attempt")); return; }
    if (lastRxID != MSG_NACK)          { failTest(F("expected NACK on 1st attempt")); return; }
    Serial.println(F("  NACK received; retrying..."));
    expectAck(MSG_RACE_MODE, &payload, 1);
}

void testMaxRetries() {
    Serial.println(F("\n[TEST] Max retries -- Responder always NACKs, expect TX_FAILED"));
    setRespMode(RESP_NACK_ALL);
    uint8_t payload = MODE_GATEDROP;
    uint8_t nacks = 0;
    for (uint8_t attempt = 0; attempt < 4; attempt++) {
        dutSend(MSG_RACE_MODE, &payload, 1);
        if (!waitResponse(ACK_TIMEOUT_MS)) break;
        if (lastRxID == MSG_NACK) nacks++;
        else break;
    }
    setRespMode(RESP_ACK_ALL);
    stats.total++;
    if (nacks >= 3) {
        Serial.print(F("  PASS: ")); Serial.print(nacks); Serial.println(F(" NACKs -> TX_FAILED"));
        stats.passed++;
    } else {
        Serial.print(F("  FAIL: only ")); Serial.print(nacks); Serial.println(F(" NACKs"));
        stats.failed++;
    }
}

void testTimeout() {
    Serial.println(F("\n[TEST] Timeout -- Responder silent, expect TX_TIMEOUT"));
    setRespMode(RESP_SILENT);
    uint8_t payload = MODE_GATEDROP;
    dutSend(MSG_RACE_MODE, &payload, 1);
    uint32_t tStart = millis();
    bool got = waitResponse(ACK_TIMEOUT_MS);
    uint32_t elapsed = millis() - tStart;
    setRespMode(RESP_ACK_ALL);
    stats.total++;
    if (!got && elapsed >= (uint32_t)ACK_TIMEOUT_MS) {
        Serial.print(F("  PASS: timeout after ")); Serial.print(elapsed); Serial.println(F(" ms"));
        stats.passed++;
    } else {
        failTest(F("expected timeout"));
    }
}

void testPriorityQueue() {
    Serial.println(F("\n[TEST] Priority -- MSG_RACE_START jumps queue"));
    // Send RACE_MODE then immediately RACE_START. Both should ACK.
    // Observe ACK IDs in serial monitor: RACE_START ACK should arrive before RACE_MODE ACK.
    uint8_t mode  = MODE_GATEDROP;
    uint8_t start = start_all;
    dutSend(MSG_RACE_MODE,  &mode,  1);
    dutSend(MSG_RACE_START, &start, 1);
    bool a1 = (waitResponse(ACK_TIMEOUT_MS) && lastRxID == MSG_ACK);
    bool a2 = (waitResponse(ACK_TIMEOUT_MS) && lastRxID == MSG_ACK);
    stats.total++;
    if (a1 && a2) { Serial.println(F("  PASS: both ACKed (observe order in log)")); stats.passed++; }
    else          { Serial.println(F("  FAIL: missed ACK")); stats.failed++; }
}

void testFullRaceSequence() {
    Serial.println(F("\n[TEST] Full race sequence: IDLE->STAGING->COUNTDOWN->RACING->COMPLETE->IDLE"));
    uint8_t states[] = { RACE_IDLE, RACE_STAGING, RACE_COUNTDOWN, RACE_RACING, RACE_COMPLETE, RACE_IDLE };
    bool pass = true;
    for (uint8_t i = 0; i < 6; i++) {
        pass &= expectAck(MSG_RACE_STATE, &states[i], 1);
        delay(TEST_DELAY_MS);
    }
    uint8_t winner = winner_leftWin;
    pass &= expectAck(MSG_WINNER, &winner, 1);
    Serial.println(pass ? F("  Sequence PASS") : F("  Sequence FAIL"));
}

void testTimingStress() {
    Serial.println(F("\n[TEST] Timing stress -- 50 x MSG_RACE_MODE"));
    uint8_t payload = MODE_GATEDROP;
    uint8_t ok = 0;
    uint32_t tStart = millis();
    for (uint8_t i = 0; i < 50; i++) {
        dutSend(MSG_RACE_MODE, &payload, 1);
        if (waitResponse(ACK_TIMEOUT_MS) && lastRxID == MSG_ACK) ok++;
        delay(10);
    }
    uint32_t elapsed = millis() - tStart;
    stats.total++;
    Serial.print(F("  ")); Serial.print(ok); Serial.print(F("/50 ACKed in "));
    Serial.print(elapsed); Serial.println(F(" ms"));
    if (ok == 50) { Serial.println(F("  PASS")); stats.passed++; }
    else          { Serial.println(F("  FAIL")); stats.failed++; }
}

void resetDUT() {
    Serial.println(F("[RESET] Resetting Responder to ACK_ALL, sending IDLE"));
    setRespMode(RESP_ACK_ALL);
    uint8_t idle = RACE_IDLE;
    dutSend(MSG_RACE_STATE, &idle, 1);
    waitResponse(ACK_TIMEOUT_MS);
    while (dut.available()) dut.read();
}

void testErrorHandling() {
    Serial.println(F("\n[ERROR HANDLING TESTS]"));
    testNackRetry();  delay(200);
    testMaxRetries(); delay(200);
    testTimeout();
}

void runAllTests() {
    Serial.println(F("\n====== DERBYTIMER PROTOCOL TEST SUITE ======"));
    resetStats();
    resetDUT();          delay(200);
    testRaceMode();      delay(200);
    testRaceState();     delay(200);
    testRaceStart();     delay(200);
    testReactionTime();  delay(200);
    testFoul();          delay(200);
    testWinner();        delay(200);
    testDisplayAdvance(); delay(200);
    testError();         delay(200);
    testNackRetry();     delay(200);
    testMaxRetries();    delay(200);
    testTimeout();       delay(200);
    testPriorityQueue(); delay(200);
    testFullRaceSequence(); delay(200);
    testTimingStress();
    printStats();
}

void printHelp() {
    Serial.println(F("\n=== DerbyTimer Protocol Tester Commands ==="));
    Serial.println(F("a - Run ALL tests"));
    Serial.println(F("1 - MSG_RACE_MODE (4 modes)"));
    Serial.println(F("2 - MSG_RACE_STATE (6 states)"));
    Serial.println(F("3 - MSG_RACE_START (priority)"));
    Serial.println(F("4 - MSG_LEFT/RIGHT_REACT (4-byte)"));
    Serial.println(F("5 - MSG_FOUL"));
    Serial.println(F("6 - MSG_WINNER"));
    Serial.println(F("7 - MSG_DISP_ADVANCE"));
    Serial.println(F("8 - MSG_ERROR (P2-15 ACK fix)"));
    Serial.println(F("9 - NACK retry"));
    Serial.println(F("t - Timing stress (50 msgs)"));
    Serial.println(F("e - Error handling (NACK/timeout)"));
    Serial.println(F("r - Reset DUT"));
    Serial.println(F("s - Full race sequence"));
    Serial.println(F("p - Print stats"));
    Serial.println(F("h - Help"));
}

// ==================== SETUP & LOOP ====================

void setup() {
    Serial.begin(BAUD_RATE);
    dut.begin(BAUD_RATE);
    delay(1000);
    Serial.println(F("\n====== DerbyTimer Serial Protocol Tester ======"));
    Serial.println(F("Wiring: D5(TX)->Responder D0(RX), D6(RX)<-Responder D1(TX)"));
    Serial.println(F("Press 'h' for commands, 'a' to run all tests"));
    resetStats();
}

void loop() {
    if (Serial.available()) {
        char cmd = Serial.read();
        switch (cmd) {
            case 'a': case 'A': runAllTests();                            break;
            case '1': resetStats(); testRaceMode();         printStats(); break;
            case '2': resetStats(); testRaceState();        printStats(); break;
            case '3': resetStats(); testRaceStart();        printStats(); break;
            case '4': resetStats(); testReactionTime();     printStats(); break;
            case '5': resetStats(); testFoul();             printStats(); break;
            case '6': resetStats(); testWinner();           printStats(); break;
            case '7': resetStats(); testDisplayAdvance();   printStats(); break;
            case '8': resetStats(); testError();            printStats(); break;
            case '9': resetStats(); testNackRetry();        printStats(); break;
            case 't': case 'T': resetStats(); testTimingStress();   printStats(); break;
            case 'e': case 'E': resetStats(); testErrorHandling();  printStats(); break;
            case 'r': case 'R': resetDUT();                           break;
            case 's': case 'S': resetStats(); testFullRaceSequence(); printStats(); break;
            case 'p': case 'P': printStats();                           break;
            case 'h': case 'H': printHelp();                            break;
            case '\n': case '\r':                                        break;
            default:
                Serial.print(F("Unknown '"));
                Serial.print(cmd);
                Serial.println(F("' -- press h for help"));
                break;
        }
    }

    // Log unsolicited messages from Responder
    if (parseDut()) {
        Serial.print(F("[UNSOLICITED] id=0x")); Serial.print(lastRxID, HEX);
        if (lastRxLen > 0) {
            Serial.print(F(" payload=0x")); Serial.print(lastRxPay[0], HEX);
        }
        Serial.println();
    }
}
