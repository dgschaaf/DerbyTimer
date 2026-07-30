// Tests for the shared Outbox engine (outbox.h): policy-table semantics
// (FATAL / TOLERATED / RETRY_ONCE) driven against the REAL serial engine --
// serialComm.cpp is compiled into this translation unit against the mock
// Arduino.h, so verdicts come from actual TX lifecycle statuses, not stubs.
// Activate: pio test -e native
#include "unity.h"
#include "arduino_mock.h"
#include "outbox.h"
#include "../../../lib/shared/serialComm.cpp"

// Policy table exercising all three policies (same rows the controllers
// will install: SC FATAL/TOLERATED entries, FC RETRY_ONCE winner).
static const OutboxEntry testEntries[] = {
    { MSG_RACE_START, OutboxPolicy::FATAL,      err_START_TX_TIMEOUT },
    { MSG_FOUL,       OutboxPolicy::FATAL,      err_STATE_TX_TIMEOUT },
    { MSG_LEFT_REACT, OutboxPolicy::TOLERATED,  err_NULL },
    { MSG_WINNER,     OutboxPolicy::RETRY_ONCE, err_NULL },
};
static Outbox ob = { testEntries, 4, 0, 0 };

// Reset all engine state between tests (same pattern as test_serialcomm.cpp).
static void resetComm() {
    Serial.reset();
    mockMillis = 0;
    rx = SerialRxState{};
    memset(txState, 0, sizeof(txState));
    txQueueLen = 0;
    ob.pendingMask = 0;
    ob.retriedMask = 0;
    rxSerial();   // clears the stale-partial tracker
}

static void feedAck(serialMsgID id) {
    uint8_t ack[] = { MSG_ACK, (uint8_t)id };
    Serial.feed(ack, 2);
}

// Drive one main-loop pass the way the controllers do: receive, send,
// then judge outcomes.
static OutboxVerdict loopOnce() {
    rxSerial();
    txService();
    return ob.checkOutcomes();
}

void setUp()    { resetComm(); }
void tearDown() {}

void test_ack_clears_tracked_message() {
    txFoulStatus(foul_left);
    ob.track(MSG_FOUL);
    TEST_ASSERT_TRUE(ob.anyPending());
    loopOnce();                        // sends
    feedAck(MSG_FOUL);
    OutboxVerdict v = loopOnce();      // ACK lands, outcome judged
    TEST_ASSERT_FALSE(v.abortRequired);
    TEST_ASSERT_FALSE(ob.anyPending());
}

void test_fatal_timeout_reports_abort_with_entry_code() {
    txRaceStart();
    ob.track(MSG_RACE_START);
    loopOnce();                        // sends
    mockMillis += txTimeout + 1;
    OutboxVerdict v = loopOnce();      // txService marks TX_TIMEOUT, engine judges
    TEST_ASSERT_TRUE(v.abortRequired);
    TEST_ASSERT_EQUAL_UINT8(err_START_TX_TIMEOUT, v.code);
    TEST_ASSERT_FALSE(ob.anyPending());   // tracking cleared with the verdict
    v = loopOnce();                    // verdict is one-shot, not sticky
    TEST_ASSERT_FALSE(v.abortRequired);
}

void test_tolerated_timeout_clears_silently() {
    txReactionTime(250000, LANE_LEFT);
    ob.track(MSG_LEFT_REACT);
    loopOnce();
    mockMillis += txTimeout + 1;
    OutboxVerdict v = loopOnce();
    TEST_ASSERT_FALSE(v.abortRequired);
    TEST_ASSERT_FALSE(ob.anyPending());
}

void test_retry_once_resends_identical_bytes() {
    txWinner(winner_leftWin);
    ob.track(MSG_WINNER);
    loopOnce();                        // original send
    uint8_t orig[8]; int origLen = Serial.txLen;
    memcpy(orig, Serial.txBuf, (size_t)origLen);

    mockMillis += txTimeout + 1;
    OutboxVerdict v = loopOnce();      // first failure: engine resends via txResend
    TEST_ASSERT_FALSE(v.abortRequired);
    TEST_ASSERT_TRUE(ob.anyPending()); // retry in flight, still tracked

    Serial.clearTx();
    loopOnce();                        // retry reaches the wire
    TEST_ASSERT_EQUAL_INT(origLen, Serial.txLen);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(orig, Serial.txBuf, origLen);
}

void test_retry_once_gives_up_silently_on_second_failure() {
    txWinner(winner_tie);
    ob.track(MSG_WINNER);
    loopOnce();                        // send
    mockMillis += txTimeout + 1;
    loopOnce();                        // first failure -> resend armed
    loopOnce();                        // retry sent
    mockMillis += txTimeout + 1;
    OutboxVerdict v = loopOnce();      // second failure -> give up
    TEST_ASSERT_FALSE(v.abortRequired);
    TEST_ASSERT_FALSE(ob.anyPending());
}

void test_retry_once_clears_on_ack_of_resend() {
    txWinner(winner_rightWin);
    ob.track(MSG_WINNER);
    loopOnce();                        // send
    mockMillis += txTimeout + 1;
    loopOnce();                        // first failure -> resend armed
    loopOnce();                        // retry sent
    feedAck(MSG_WINNER);
    OutboxVerdict v = loopOnce();
    TEST_ASSERT_FALSE(v.abortRequired);
    TEST_ASSERT_FALSE(ob.anyPending());
}

void test_anypending_true_while_unresolved() {
    txFoulStatus(foul_right);
    ob.track(MSG_FOUL);
    for (int i = 0; i < 5; i++) {
        OutboxVerdict v = loopOnce();  // sent, no ACK, no timeout yet
        TEST_ASSERT_FALSE(v.abortRequired);
        TEST_ASSERT_TRUE(ob.anyPending());
    }
}

void test_untracked_message_ignored() {
    txRaceMode(MODE_PRO);              // not in the policy table
    ob.track(MSG_RACE_MODE);           // must be a no-op
    TEST_ASSERT_FALSE(ob.anyPending());
    loopOnce();
    mockMillis += txTimeout + 1;
    OutboxVerdict v = loopOnce();      // its timeout is nobody's business
    TEST_ASSERT_FALSE(v.abortRequired);
    TEST_ASSERT_FALSE(ob.anyPending());
}

void test_track_rearms_retry_budget_for_new_heat() {
    // Heat 1: winner exhausts its retry and gives up.
    txWinner(winner_leftWin);
    ob.track(MSG_WINNER);
    loopOnce();
    mockMillis += txTimeout + 1;
    loopOnce();                        // failure 1 -> retry
    loopOnce();                        // retry sent
    mockMillis += txTimeout + 1;
    loopOnce();                        // failure 2 -> gave up
    TEST_ASSERT_FALSE(ob.anyPending());
    loopOnce();                        // idle pass: dead queue entry dequeued

    // Heat 2: a fresh track() must reset the retry budget.
    txWinner(winner_rightWin);
    ob.track(MSG_WINNER);
    loopOnce();                        // send
    mockMillis += txTimeout + 1;
    loopOnce();                        // failure 1 -> retry available again
    TEST_ASSERT_TRUE(ob.anyPending());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_ack_clears_tracked_message);
    RUN_TEST(test_fatal_timeout_reports_abort_with_entry_code);
    RUN_TEST(test_tolerated_timeout_clears_silently);
    RUN_TEST(test_retry_once_resends_identical_bytes);
    RUN_TEST(test_retry_once_gives_up_silently_on_second_failure);
    RUN_TEST(test_retry_once_clears_on_ack_of_resend);
    RUN_TEST(test_anypending_true_while_unresolved);
    RUN_TEST(test_untracked_message_ignored);
    RUN_TEST(test_track_rearms_retry_budget_for_new_heat);
    return UNITY_END();
}
