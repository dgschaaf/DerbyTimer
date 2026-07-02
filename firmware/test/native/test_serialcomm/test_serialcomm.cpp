// Tests for the serial protocol engine (rxSerial / TX queue) in serialComm.cpp.
// The production .cpp is compiled into this translation unit against the mock
// Arduino.h, so the real parser and TX state machine are under test -- not a copy.
// Activate: pio test -e native
#include "unity.h"
#include "Arduino.h"
#include "raceTypes.h"
#include "serialComm.h"
#include "../../../lib/shared/serialComm.cpp"

// Reset all engine state between tests. The stale-partial tracker inside
// rxSerial() is a function-local static; calling rxSerial() on an empty
// stream resets it.
static void resetComm() {
    Serial.reset();
    mockMillis = 0;
    rx = SerialRxState{};
    memset(txState, 0, sizeof(txState));
    txQueueLen = 0;
    rxSerial();   // clears the stale-partial tracker
}

void setUp()    { resetComm(); }
void tearDown() {}

// ---------------- RX parsing ----------------

void test_rx_race_state_sets_value_flag_and_acks() {
    uint8_t msg[] = { MSG_RACE_STATE, RACE_STAGING };
    Serial.feed(msg, 2);
    TEST_ASSERT_TRUE(rxSerial());
    TEST_ASSERT_EQUAL_UINT8(RACE_STAGING, rx.State);
    TEST_ASSERT_TRUE(rx.StateChanged);
    TEST_ASSERT_EQUAL_INT(2, Serial.txLen);
    TEST_ASSERT_EQUAL_UINT8(MSG_ACK, Serial.txBuf[0]);
    TEST_ASSERT_EQUAL_UINT8(MSG_RACE_STATE, Serial.txBuf[1]);
}

void test_rx_race_start_zero_payload() {
    Serial.feedByte(MSG_RACE_START);
    TEST_ASSERT_TRUE(rxSerial());
    TEST_ASSERT_TRUE(rx.RaceStart);
    TEST_ASSERT_EQUAL_UINT8(MSG_ACK, Serial.txBuf[0]);
}

void test_rx_partial_message_left_in_buffer() {
    // 2 of 4 reaction payload bytes: nothing consumed, no response sent
    uint8_t msg[] = { MSG_LEFT_REACT, 0x01, 0x02 };
    Serial.feed(msg, 3);
    TEST_ASSERT_FALSE(rxSerial());
    TEST_ASSERT_EQUAL_INT(3, Serial.available());
    TEST_ASSERT_EQUAL_INT(0, Serial.txLen);
}

void test_rx_stale_partial_flushed_and_nacked() {
    uint8_t msg[] = { MSG_LEFT_REACT, 0x01, 0x02 };
    Serial.feed(msg, 3);
    rxSerial();                                  // registers the stall
    mockMillis += stalePartialTimeoutMs + 1;
    rxSerial();                                  // flushes the stale ID + NACKs
    TEST_ASSERT_EQUAL_INT(2, Serial.available());  // ID byte dropped
    TEST_ASSERT_EQUAL_UINT8(MSG_NACK, Serial.txBuf[0]);
    TEST_ASSERT_EQUAL_UINT8(MSG_LEFT_REACT, Serial.txBuf[1]);
    TEST_ASSERT_EQUAL_UINT8(err_INVALID_MSG, rx.lastLocalError);
}

void test_rx_partial_that_completes_is_not_flushed() {
    uint8_t head[] = { MSG_LEFT_REACT, 0x10, 0x27 };   // 10000 LE, partial
    Serial.feed(head, 3);
    rxSerial();                                  // stall registered
    mockMillis += stalePartialTimeoutMs - 10;    // not yet stale
    uint8_t tail[] = { 0x00, 0x00 };
    Serial.feed(tail, 2);                        // payload completes
    TEST_ASSERT_TRUE(rxSerial());
    TEST_ASSERT_TRUE(rx.LeftReactionValid);
    TEST_ASSERT_EQUAL_INT32(10000, rx.LeftReactionTime);
}

void test_rx_unknown_id_discarded_silently() {
    Serial.feedByte(0xFF);
    TEST_ASSERT_FALSE(rxSerial());
    TEST_ASSERT_EQUAL_INT(0, Serial.available());   // byte consumed
    TEST_ASSERT_EQUAL_INT(0, Serial.txLen);         // no NACK for garbage
    TEST_ASSERT_EQUAL_UINT8(err_INVALID_MSG, rx.lastLocalError);
}

void test_rx_reaction_negative_rejected_with_nack() {
    int32_t bad = -5000;
    uint8_t msg[5] = { MSG_LEFT_REACT };
    memcpy(&msg[1], &bad, 4);
    Serial.feed(msg, 5);
    rxSerial();
    TEST_ASSERT_FALSE(rx.LeftReactionValid);
    TEST_ASSERT_EQUAL_UINT8(MSG_NACK, Serial.txBuf[0]);
}

void test_rx_reaction_over_max_rejected_with_nack() {
    int32_t bad = (int32_t)(maxValidReactionUs + 1);
    uint8_t msg[5] = { MSG_RIGHT_REACT };
    memcpy(&msg[1], &bad, 4);
    Serial.feed(msg, 5);
    rxSerial();
    TEST_ASSERT_FALSE(rx.RightReactionValid);
    TEST_ASSERT_EQUAL_UINT8(MSG_NACK, Serial.txBuf[0]);
}

void test_rx_reaction_valid_accepted_and_acked() {
    int32_t good = 250000;
    uint8_t msg[5] = { MSG_LEFT_REACT };
    memcpy(&msg[1], &good, 4);
    Serial.feed(msg, 5);
    rxSerial();
    TEST_ASSERT_TRUE(rx.LeftReactionValid);
    TEST_ASSERT_EQUAL_INT32(250000, rx.LeftReactionTime);
    TEST_ASSERT_EQUAL_UINT8(MSG_ACK, Serial.txBuf[0]);
}

// ---------------- TX engine ----------------

void test_tx_send_then_ack_lifecycle() {
    TEST_ASSERT_TRUE(txRaceMode(MODE_PRO));
    txService();   // sends
    TEST_ASSERT_EQUAL_INT(2, Serial.txLen);
    TEST_ASSERT_EQUAL_UINT8(MSG_RACE_MODE, Serial.txBuf[0]);
    TEST_ASSERT_EQUAL_UINT8(MODE_PRO, Serial.txBuf[1]);
    TEST_ASSERT_EQUAL(TX_SENT, txStatusOf(MSG_RACE_MODE));

    uint8_t ack[] = { MSG_ACK, MSG_RACE_MODE };
    Serial.feed(ack, 2);
    rxSerial();
    TEST_ASSERT_EQUAL(TX_ACKED, txStatusOf(MSG_RACE_MODE));
    txService();   // dequeues the terminal entry
    TEST_ASSERT_EQUAL_UINT8(0, txQueueLen);
}

void test_tx_timeout_after_no_ack() {
    txRaceMode(MODE_PRO);
    txService();
    mockMillis += txTimeout + 1;
    txService();
    TEST_ASSERT_EQUAL(TX_TIMEOUT, txStatusOf(MSG_RACE_MODE));
}

void test_tx_nack_retries_then_fails() {
    txRaceMode(MODE_PRO);
    int sends = 0;
    for (int i = 0; i < 10 && txStatusOf(MSG_RACE_MODE) != TX_FAILED; i++) {
        Serial.clearTx();
        txService();                       // (re)send if due
        if (Serial.txLen > 0) {
            sends++;
            uint8_t nack[] = { MSG_NACK, MSG_RACE_MODE };
            Serial.feed(nack, 2);
            rxSerial();                    // peer NACKs every attempt
        }
    }
    TEST_ASSERT_EQUAL(TX_FAILED, txStatusOf(MSG_RACE_MODE));
    TEST_ASSERT_EQUAL_INT(4, sends);       // 1 initial + 3 retries
}

void test_tx_priority_message_jumps_queue() {
    txRaceMode(MODE_PRO);
    txWinner(winner_leftWin);
    txRaceStart();                          // priority: inserted at front
    txService();
    TEST_ASSERT_EQUAL_UINT8(MSG_RACE_START, Serial.txBuf[0]);
}

void test_tx_duplicate_enqueue_rejected() {
    TEST_ASSERT_TRUE(txRaceMode(MODE_PRO));
    TEST_ASSERT_FALSE(txRaceMode(MODE_GATEDROP));   // already queued
}

void test_tx_payload_captured_at_enqueue() {
    uint32_t reaction = 123456;
    txReactionTime(reaction, LANE_LEFT);
    reaction = 0;                           // mutate after enqueue -- must not matter
    txService();
    TEST_ASSERT_EQUAL_INT(5, Serial.txLen);
    TEST_ASSERT_EQUAL_UINT8(MSG_LEFT_REACT, Serial.txBuf[0]);
    uint32_t wire;
    memcpy(&wire, &Serial.txBuf[1], 4);
    TEST_ASSERT_EQUAL_UINT32(123456, wire);
}

void test_tx_single_in_flight() {
    txRaceMode(MODE_PRO);
    txWinner(winner_tie);
    txService();
    int lenAfterFirst = Serial.txLen;
    txService();   // second message must wait for the first to resolve
    TEST_ASSERT_EQUAL_INT(lenAfterFirst, Serial.txLen);
}

// ---------------- txResend ----------------

void test_txresend_after_timeout_resends_identical_bytes() {
    txWinner(winner_leftWin);
    txService();                                   // original send
    uint8_t orig[8]; int origLen = Serial.txLen;
    memcpy(orig, Serial.txBuf, (size_t)origLen);
    mockMillis += txTimeout + 1;
    txService();                                   // marks TX_TIMEOUT
    txService();                                   // dequeues the terminal entry
    Serial.clearTx();
    TEST_ASSERT_TRUE(txResend(MSG_WINNER));
    txService();                                   // resend from captured payload
    TEST_ASSERT_EQUAL_INT(origLen, Serial.txLen);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(orig, Serial.txBuf, origLen);
}

void test_txresend_same_pass_as_timeout_detection() {
    // The realistic caller window: a per-loop outcome check sees TX_TIMEOUT
    // in the same pass txService() marked it, BEFORE the dequeue pass. The
    // entry is still queued, so a plain re-enqueue would be rejected as a
    // duplicate; txResend must rearm it in place instead.
    txWinner(winner_tie);
    txService();
    mockMillis += txTimeout + 1;
    txService();                                   // TX_TIMEOUT, still queued
    TEST_ASSERT_TRUE(txResend(MSG_WINNER));
    Serial.clearTx();
    txService();                                   // resends
    TEST_ASSERT_EQUAL_INT(2, Serial.txLen);
    TEST_ASSERT_EQUAL_UINT8(MSG_WINNER, Serial.txBuf[0]);
    TEST_ASSERT_EQUAL(TX_SENT, txStatusOf(MSG_WINNER));
}

void test_txresend_rejected_while_in_flight() {
    txWinner(winner_leftWin);
    txService();                                   // TX_SENT
    TEST_ASSERT_FALSE(txResend(MSG_WINNER));
}

void test_txresend_rejected_for_never_sent_id() {
    TEST_ASSERT_FALSE(txResend(MSG_WINNER));       // slot never used
    TEST_ASSERT_FALSE(txResend(MSG_COUNT));        // out of range
}

void test_txresend_acks_to_acked_normally() {
    txWinner(winner_rightWin);
    txService();
    mockMillis += txTimeout + 1;
    txService();                                   // timeout
    txService();                                   // dequeue
    TEST_ASSERT_TRUE(txResend(MSG_WINNER));
    txService();                                   // resend
    uint8_t ack[] = { MSG_ACK, MSG_WINNER };
    Serial.feed(ack, 2);
    rxSerial();
    TEST_ASSERT_EQUAL(TX_ACKED, txStatusOf(MSG_WINNER));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_rx_race_state_sets_value_flag_and_acks);
    RUN_TEST(test_rx_race_start_zero_payload);
    RUN_TEST(test_rx_partial_message_left_in_buffer);
    RUN_TEST(test_rx_stale_partial_flushed_and_nacked);
    RUN_TEST(test_rx_partial_that_completes_is_not_flushed);
    RUN_TEST(test_rx_unknown_id_discarded_silently);
    RUN_TEST(test_rx_reaction_negative_rejected_with_nack);
    RUN_TEST(test_rx_reaction_over_max_rejected_with_nack);
    RUN_TEST(test_rx_reaction_valid_accepted_and_acked);
    RUN_TEST(test_tx_send_then_ack_lifecycle);
    RUN_TEST(test_tx_timeout_after_no_ack);
    RUN_TEST(test_tx_nack_retries_then_fails);
    RUN_TEST(test_tx_priority_message_jumps_queue);
    RUN_TEST(test_tx_duplicate_enqueue_rejected);
    RUN_TEST(test_tx_payload_captured_at_enqueue);
    RUN_TEST(test_tx_single_in_flight);
    RUN_TEST(test_txresend_after_timeout_resends_identical_bytes);
    RUN_TEST(test_txresend_same_pass_as_timeout_detection);
    RUN_TEST(test_txresend_rejected_while_in_flight);
    RUN_TEST(test_txresend_rejected_for_never_sent_id);
    RUN_TEST(test_txresend_acks_to_acked_normally);
    return UNITY_END();
}
