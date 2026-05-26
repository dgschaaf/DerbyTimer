// Tests for computeHeatResults() in finishController.cpp.
// Activate: pio test -e native (requires P2-35 env setup and P2-12 extraction)
#include "unity.h"
#include "arduino_mock.h"

void setUp() {}
void tearDown() {}

void test_no_foul_car_time_equals_race_minus_reaction() {
    // carTime = raceTime - reactionTime when no foul
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (extract computeHeatResults)");
}

void test_foul_car_time_equals_race_plus_reaction() {
    // carTime = raceTime + reactionTime when foul (car was rolling early)
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (extract computeHeatResults)");
}

void test_both_foul_winner_is_no_result() {
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (extract computeHeatResults)");
}

void test_one_foul_other_wins() {
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (extract computeHeatResults)");
}

void test_lower_car_time_wins() {
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (extract computeHeatResults)");
}

void test_equal_car_time_is_tie() {
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (extract computeHeatResults)");
}

void test_dnf_ignored_in_winner_logic() {
    // DNF lane loses; non-DNF lane wins regardless of time
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (extract computeHeatResults)");
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_no_foul_car_time_equals_race_minus_reaction);
    RUN_TEST(test_foul_car_time_equals_race_plus_reaction);
    RUN_TEST(test_both_foul_winner_is_no_result);
    RUN_TEST(test_one_foul_other_wins);
    RUN_TEST(test_lower_car_time_wins);
    RUN_TEST(test_equal_car_time_is_tie);
    RUN_TEST(test_dnf_ignored_in_winner_logic);
    return UNITY_END();
}
