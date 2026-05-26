// Tests for raceTimingData::isFoul() and raceTimingData::reactionTimeUs()
// in startController.cpp.
// Activate: pio test -e native (requires P2-35 env setup and P2-12 extraction)
#include "unity.h"
#include "arduino_mock.h"

void setUp() {}
void tearDown() {}

void test_foul_trigger_before_race_start() {
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (extract state handlers)");
}

void test_foul_trigger_after_race_start() {
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (extract state handlers)");
}

void test_foul_trigger_exact_boundary_not_foul() {
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (extract state handlers)");
}

void test_reaction_early_trigger_positive() {
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (extract state handlers)");
}

void test_reaction_late_trigger_positive() {
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (extract state handlers)");
}

void test_reaction_zero_delta() {
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (extract state handlers)");
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_foul_trigger_before_race_start);
    RUN_TEST(test_foul_trigger_after_race_start);
    RUN_TEST(test_foul_trigger_exact_boundary_not_foul);
    RUN_TEST(test_reaction_early_trigger_positive);
    RUN_TEST(test_reaction_late_trigger_positive);
    RUN_TEST(test_reaction_zero_delta);
    return UNITY_END();
}
