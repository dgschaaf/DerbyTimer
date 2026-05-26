// Tests for stateMachine::allowedTransition() -- all legal and a sample of
// illegal state pairs. stateMachine is defined in firmware/lib/shared/stateMachine.h.
// Activate: pio test -e native (requires P2-35 env setup)
//
// Note: stateMachine includes serialComm.h which pulls in Arduino Serial calls.
// These tests will require arduino_mock.h to stub Serial, or a targeted mock of
// the serialComm functions (txRaceState, txNack, txStatusOf). Mark as IGNORE
// until that stub layer is wired in P2-35.
#include "unity.h"
#include "arduino_mock.h"

void setUp() {}
void tearDown() {}

// Legal transitions (SC initiates, FC follows)
void test_idle_to_staging_allowed() {
    TEST_IGNORE_MESSAGE("Stub -- stateMachine needs serialComm stubs before it can link natively (P2-35)");
}

void test_staging_to_idle_allowed() {
    TEST_IGNORE_MESSAGE("Stub -- stateMachine needs serialComm stubs before it can link natively (P2-35)");
}

void test_staging_to_countdown_allowed() {
    TEST_IGNORE_MESSAGE("Stub -- stateMachine needs serialComm stubs before it can link natively (P2-35)");
}

void test_countdown_to_racing_allowed() {
    TEST_IGNORE_MESSAGE("Stub -- stateMachine needs serialComm stubs before it can link natively (P2-35)");
}

// Legal transitions (FC initiates, SC follows)
void test_racing_to_complete_allowed() {
    TEST_IGNORE_MESSAGE("Stub -- stateMachine needs serialComm stubs before it can link natively (P2-35)");
}

void test_complete_to_idle_allowed() {
    TEST_IGNORE_MESSAGE("Stub -- stateMachine needs serialComm stubs before it can link natively (P2-35)");
}

// Illegal transitions
void test_idle_to_racing_not_allowed() {
    TEST_IGNORE_MESSAGE("Stub -- stateMachine needs serialComm stubs before it can link natively (P2-35)");
}

void test_idle_to_complete_not_allowed() {
    TEST_IGNORE_MESSAGE("Stub -- stateMachine needs serialComm stubs before it can link natively (P2-35)");
}

void test_racing_to_idle_not_allowed() {
    TEST_IGNORE_MESSAGE("Stub -- stateMachine needs serialComm stubs before it can link natively (P2-35)");
}

void test_complete_to_staging_not_allowed() {
    TEST_IGNORE_MESSAGE("Stub -- stateMachine needs serialComm stubs before it can link natively (P2-35)");
}

void test_countdown_to_staging_not_allowed() {
    TEST_IGNORE_MESSAGE("Stub -- stateMachine needs serialComm stubs before it can link natively (P2-35)");
}

void test_same_state_not_allowed() {
    TEST_IGNORE_MESSAGE("Stub -- stateMachine needs serialComm stubs before it can link natively (P2-35)");
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
    RUN_TEST(test_racing_to_idle_not_allowed);
    RUN_TEST(test_complete_to_staging_not_allowed);
    RUN_TEST(test_countdown_to_staging_not_allowed);
    RUN_TEST(test_same_state_not_allowed);
    return UNITY_END();
}
