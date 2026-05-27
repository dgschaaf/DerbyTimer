// Tests for stateMachine::allowedTransition() -- all legal and a sample of
// illegal state pairs. stateMachine is defined in firmware/lib/shared/stateMachine.h.
// Activate: pio test -e native (requires P2-35 env setup)
#include "unity.h"
#include "arduino_mock.h"
#include "stateMachine.h"

// Stubs so stateMachine's TX calls link without real serial hardware.
SerialRxState rx;
bool     txRaceState(uint8_t)    { return false; }
txStatus txStatusOf(serialMsgID) { return TX_NONE; }
void     txNack(uint8_t)         {}

void setUp() {}
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
    return UNITY_END();
}
