// Tests for the shared stateMachine: the allowedTransition() legality table,
// plus the request/service/takeEntry/takeExit declare-intent interface
// driven against the REAL serial engine -- serialComm.cpp is compiled into
// this translation unit against the mock Arduino.h, so the full coordinated
// transition protocol (enqueue, send, ACK, commit/revert) is under test, not
// a copy. stateMachine is defined in firmware/lib/shared/stateMachine.h.
// Activate: pio test -e native
#include "unity.h"
#include "arduino_mock.h"
#include "stateMachine.h"
#include "../../../lib/shared/serialComm.cpp"

// Reset all engine state between tests (same pattern as test_serialcomm.cpp).
static void resetComm() {
    Serial.reset();
    mockMillis = 0;
    rx = SerialRxState{};
    memset(txState, 0, sizeof(txState));
    txQueueLen = 0;
    rxSerial();   // clears the stale-partial tracker
}

// One firmware main-loop pass, in the order both controllers use:
// rxSerial() -> handler (stm.service()) -> txService().
static void loopOnce(stateMachine& sm, bool holdRx = false) {
    rxSerial();
    sm.service(holdRx);
    txService();
}

static void feedAck(serialMsgID id) {
    uint8_t ack[] = { MSG_ACK, (uint8_t)id };
    Serial.feed(ack, 2);
}

static void feedState(raceState s) {
    uint8_t msg[] = { MSG_RACE_STATE, (uint8_t)s };
    Serial.feed(msg, 2);
}

void setUp()    { resetComm(); }
void tearDown() {}

// Legal transitions (SC initiates, FC follows)
void test_idle_to_staging_allowed() {
    stateMachine sm = {}; sm.current = RACE_IDLE;
    TEST_ASSERT_TRUE(sm.allowedTransition(RACE_STAGING));
}

void test_staging_to_idle_allowed() {
    stateMachine sm = {}; sm.current = RACE_STAGING;
    TEST_ASSERT_TRUE(sm.allowedTransition(RACE_IDLE));
}

void test_staging_to_countdown_allowed() {
    stateMachine sm = {}; sm.current = RACE_STAGING;
    TEST_ASSERT_TRUE(sm.allowedTransition(RACE_COUNTDOWN));
}

void test_countdown_to_racing_allowed() {
    stateMachine sm = {}; sm.current = RACE_COUNTDOWN;
    TEST_ASSERT_TRUE(sm.allowedTransition(RACE_RACING));
}

// Legal transitions (FC initiates, SC follows)
void test_racing_to_complete_allowed() {
    stateMachine sm = {}; sm.current = RACE_RACING;
    TEST_ASSERT_TRUE(sm.allowedTransition(RACE_COMPLETE));
}

void test_complete_to_idle_allowed() {
    stateMachine sm = {}; sm.current = RACE_COMPLETE;
    TEST_ASSERT_TRUE(sm.allowedTransition(RACE_IDLE));
}

// Illegal transitions
void test_idle_to_racing_not_allowed() {
    stateMachine sm = {}; sm.current = RACE_IDLE;
    TEST_ASSERT_FALSE(sm.allowedTransition(RACE_RACING));
}

void test_idle_to_complete_not_allowed() {
    stateMachine sm = {}; sm.current = RACE_IDLE;
    TEST_ASSERT_FALSE(sm.allowedTransition(RACE_COMPLETE));
}

void test_racing_to_staging_not_allowed() {
    // RACING->IDLE is the emergency abort (allowed); RACING->STAGING is not
    stateMachine sm = {}; sm.current = RACE_RACING;
    TEST_ASSERT_FALSE(sm.allowedTransition(RACE_STAGING));
}

void test_complete_to_staging_not_allowed() {
    stateMachine sm = {}; sm.current = RACE_COMPLETE;
    TEST_ASSERT_FALSE(sm.allowedTransition(RACE_STAGING));
}

void test_countdown_to_staging_not_allowed() {
    stateMachine sm = {}; sm.current = RACE_COUNTDOWN;
    TEST_ASSERT_FALSE(sm.allowedTransition(RACE_STAGING));
}

void test_same_state_not_allowed() {
    stateMachine sm = {}; sm.current = RACE_IDLE;
    TEST_ASSERT_FALSE(sm.allowedTransition(RACE_IDLE));
}

// ---------------- Declare-intent interface ----------------

// Initiator path: request() records intent, service() carries the
// coordinated transition -- MSG_RACE_STATE on the wire, commit only after
// the peer ACKs, entry/exit each observable exactly once.
void test_request_commits_on_ack() {
    stateMachine sm = {}; sm.current = RACE_IDLE;
    sm.request(RACE_STAGING);
    TEST_ASSERT_EQUAL(RACE_STAGING, sm.target);   // intent recorded...
    TEST_ASSERT_EQUAL(RACE_IDLE, sm.current);     // ...nothing committed yet

    loopOnce(sm);   // service enqueues, txService sends
    TEST_ASSERT_EQUAL_INT(2, Serial.txLen);
    TEST_ASSERT_EQUAL_UINT8(MSG_RACE_STATE, Serial.txBuf[0]);
    TEST_ASSERT_EQUAL_UINT8(RACE_STAGING, Serial.txBuf[1]);
    TEST_ASSERT_EQUAL(RACE_IDLE, sm.current);     // no commit before ACK

    feedAck(MSG_RACE_STATE);
    loopOnce(sm);   // ACK lands -> commit
    TEST_ASSERT_EQUAL(RACE_STAGING, sm.current);
    TEST_ASSERT_TRUE(sm.takeEntry());
    TEST_ASSERT_FALSE(sm.takeEntry());   // one-shot
    TEST_ASSERT_TRUE(sm.takeExit());
    TEST_ASSERT_FALSE(sm.takeExit());
}

// No ACK: the request stays pending (never commits), then the TX timeout
// abandons it -- target reverts to current, no entry flag ever raised.
void test_request_without_ack_reverts_on_timeout() {
    stateMachine sm = {}; sm.current = RACE_IDLE;
    sm.request(RACE_STAGING);
    for (int i = 0; i < 5; i++) loopOnce(sm);
    TEST_ASSERT_EQUAL(RACE_IDLE, sm.current);       // never commits without ACK
    TEST_ASSERT_EQUAL(RACE_STAGING, sm.target);     // still trying

    mockMillis += txTimeout + 1;
    loopOnce(sm);   // txService marks TX_TIMEOUT
    loopOnce(sm);   // service sees it and abandons the request
    TEST_ASSERT_EQUAL(RACE_IDLE, sm.current);
    TEST_ASSERT_EQUAL(RACE_IDLE, sm.target);
    TEST_ASSERT_FALSE(sm.takeEntry());
}

// Illegal request (IDLE -> COMPLETE): ignored entirely, nothing on the wire.
void test_illegal_request_ignored() {
    stateMachine sm = {}; sm.current = RACE_IDLE;
    sm.request(RACE_COMPLETE);
    TEST_ASSERT_EQUAL(RACE_IDLE, sm.target);   // intent not even recorded
    for (int i = 0; i < 3; i++) loopOnce(sm);
    TEST_ASSERT_EQUAL(RACE_IDLE, sm.current);
    TEST_ASSERT_EQUAL_INT(0, Serial.txLen);    // wire silent
}

// Follower path: a received MSG_RACE_STATE commits via service() without
// re-broadcasting -- the only wire traffic is the ACK rxSerial() emits.
void test_follower_commits_without_initiating() {
    stateMachine sm = {}; sm.current = RACE_IDLE;
    feedState(RACE_STAGING);
    loopOnce(sm);
    TEST_ASSERT_EQUAL(RACE_STAGING, sm.current);
    TEST_ASSERT_TRUE(sm.takeEntry());
    TEST_ASSERT_EQUAL_INT(2, Serial.txLen);
    TEST_ASSERT_EQUAL_UINT8(MSG_ACK, Serial.txBuf[0]);
    TEST_ASSERT_EQUAL_UINT8(MSG_RACE_STATE, Serial.txBuf[1]);
}

// The RACING data-integrity hold: service(true) DEFERS a received state
// edge -- it must not transition and must not discard the message. The
// first unheld pass applies it.
void test_holdrx_defers_edge_without_discarding() {
    stateMachine sm = {}; sm.current = RACE_RACING; sm.target = RACE_RACING;
    feedState(RACE_COMPLETE);
    loopOnce(sm, true);
    TEST_ASSERT_EQUAL(RACE_RACING, sm.current);   // held
    TEST_ASSERT_TRUE(rx.StateChanged);            // deferred, NOT discarded
    loopOnce(sm, false);
    TEST_ASSERT_EQUAL(RACE_COMPLETE, sm.current); // applied once released
    TEST_ASSERT_TRUE(sm.takeEntry());
}

// Stale-State regression (the historical P0): rx.State is level-triggered
// and still holds the previous heat's IDLE after its edge was consumed.
// Repeated service() calls must NOT re-fire it -- RACING->IDLE is a legal
// transition, so a re-fire would abort the heat mid-race.
void test_stale_rx_state_not_refired() {
    stateMachine sm = {}; sm.current = RACE_RACING; sm.target = RACE_RACING;
    rx.State        = RACE_IDLE;   // stale value from the previous heat
    rx.StateChanged = false;       // its edge was already consumed
    for (int i = 0; i < 10; i++) loopOnce(sm);
    TEST_ASSERT_EQUAL(RACE_RACING, sm.current);
    TEST_ASSERT_FALSE(sm.takeEntry());
}

// forceIdle() with the new accessors: entry observable exactly once, exit
// never raised (abort path skips exit work by design).
void test_forceidle_take_entry_once_no_exit() {
    stateMachine sm = {}; sm.current = RACE_COUNTDOWN; sm.target = RACE_COUNTDOWN;
    sm.forceIdle();
    TEST_ASSERT_EQUAL(RACE_IDLE, sm.current);
    TEST_ASSERT_TRUE(sm.takeEntry());
    TEST_ASSERT_FALSE(sm.takeEntry());
    TEST_ASSERT_FALSE(sm.takeExit());
}

// Equivalence: request+service must be wire- and state-identical to the old
// handler ritual (serviceRx / guard / selfTransition). Same scenario through
// both interfaces; compare everything that leaves the machine.
static void runOldRitual(stateMachine& sm, raceState goal) {
    rxSerial();
    sm.serviceRx();
    if (goal != sm.current) sm.selfTransition(goal);
    txService();
}

void test_request_service_matches_old_interface() {
    // Old interface: IDLE -> STAGING with an ACK after the first send
    stateMachine oldSm = {}; oldSm.current = RACE_IDLE;
    runOldRitual(oldSm, RACE_STAGING);
    feedAck(MSG_RACE_STATE);
    runOldRitual(oldSm, RACE_STAGING);
    uint8_t oldWire[8]; int oldWireLen = Serial.txLen;
    memcpy(oldWire, Serial.txBuf, (size_t)oldWireLen);
    raceState oldCurrent = oldSm.current;
    bool oldEntry = oldSm.entry, oldExit = oldSm.exit;

    resetComm();

    // New interface: same scenario
    stateMachine newSm = {}; newSm.current = RACE_IDLE;
    newSm.request(RACE_STAGING);
    loopOnce(newSm);
    feedAck(MSG_RACE_STATE);
    loopOnce(newSm);

    TEST_ASSERT_EQUAL_INT(oldWireLen, Serial.txLen);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(oldWire, Serial.txBuf, oldWireLen);
    TEST_ASSERT_EQUAL(oldCurrent, newSm.current);
    TEST_ASSERT_EQUAL(oldEntry, newSm.entry);
    TEST_ASSERT_EQUAL(oldExit, newSm.exit);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_idle_to_staging_allowed);
    RUN_TEST(test_staging_to_idle_allowed);
    RUN_TEST(test_staging_to_countdown_allowed);
    RUN_TEST(test_countdown_to_racing_allowed);
    RUN_TEST(test_racing_to_complete_allowed);
    RUN_TEST(test_complete_to_idle_allowed);
    RUN_TEST(test_idle_to_racing_not_allowed);
    RUN_TEST(test_idle_to_complete_not_allowed);
    RUN_TEST(test_racing_to_staging_not_allowed);
    RUN_TEST(test_complete_to_staging_not_allowed);
    RUN_TEST(test_countdown_to_staging_not_allowed);
    RUN_TEST(test_same_state_not_allowed);
    RUN_TEST(test_request_commits_on_ack);
    RUN_TEST(test_request_without_ack_reverts_on_timeout);
    RUN_TEST(test_illegal_request_ignored);
    RUN_TEST(test_follower_commits_without_initiating);
    RUN_TEST(test_stale_rx_state_not_refired);
    RUN_TEST(test_holdrx_defers_edge_without_discarding);
    RUN_TEST(test_forceidle_take_entry_once_no_exit);
    RUN_TEST(test_request_service_matches_old_interface);
    return UNITY_END();
}
