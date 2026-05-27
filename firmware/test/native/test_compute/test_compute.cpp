// Tests for computeHeatResults() in finishController.h.
// Activate: pio test -e native (requires P2-35 env setup)
#include "unity.h"
#include "arduino_mock.h"
#include "finishController.h"

void setUp() {}
void tearDown() {}

void test_no_foul_car_time_equals_race_minus_reaction() {
    HeatResults r = {};
    r.left.reactionTimeUs = 100000;
    TimingInputs t = {};
    t.leftTimeUs = 5000000;
    computeHeatResults(r, t);
    TEST_ASSERT_EQUAL_UINT32(4900000, r.left.carTimeUs);
}

void test_foul_car_time_equals_race_plus_reaction() {
    HeatResults r = {};
    r.left.foul = true;
    r.left.reactionTimeUs = 100000;
    TimingInputs t = {};
    t.leftTimeUs = 5000000;
    computeHeatResults(r, t);
    TEST_ASSERT_EQUAL_UINT32(5100000, r.left.carTimeUs);
}

void test_both_foul_winner_is_no_result() {
    HeatResults r = {};
    r.left.foul = true; r.right.foul = true;
    TimingInputs t = {};
    t.leftTimeUs = 4000000; t.rightTimeUs = 5000000;
    computeHeatResults(r, t);
    TEST_ASSERT_FALSE(r.left.winner);
    TEST_ASSERT_FALSE(r.right.winner);
}

void test_one_foul_other_wins() {
    // left fouls but had faster raw time -- right still wins
    HeatResults r = {};
    r.left.foul = true;
    TimingInputs t = {};
    t.leftTimeUs = 3000000; t.rightTimeUs = 5000000;
    computeHeatResults(r, t);
    TEST_ASSERT_FALSE(r.left.winner);
    TEST_ASSERT_TRUE(r.right.winner);
}

void test_lower_car_time_wins() {
    HeatResults r = {};
    r.left.reactionTimeUs  = 200000;
    r.right.reactionTimeUs = 100000;
    TimingInputs t = {};
    t.leftTimeUs  = 5000000;   // left  carTime = 4800000
    t.rightTimeUs = 4800000;   // right carTime = 4700000
    computeHeatResults(r, t);
    TEST_ASSERT_FALSE(r.left.winner);
    TEST_ASSERT_TRUE(r.right.winner);
}

void test_equal_car_time_is_tie() {
    // Neither winner flag set; caller detects tie via winnerMask logic
    HeatResults r = {};
    r.left.reactionTimeUs  = 100000;
    r.right.reactionTimeUs = 100000;
    TimingInputs t = {};
    t.leftTimeUs = 5000000; t.rightTimeUs = 5000000;
    computeHeatResults(r, t);
    TEST_ASSERT_FALSE(r.left.winner);
    TEST_ASSERT_FALSE(r.right.winner);
}

void test_dnf_ignored_in_winner_logic() {
    // DNF lane loses even if it would have had a faster time
    HeatResults r = {};
    TimingInputs t = {};
    t.leftDnf    = true;
    t.leftTimeUs = 3000000;   // fast but DNF
    t.rightTimeUs = 9000000;  // slow but finished
    computeHeatResults(r, t);
    TEST_ASSERT_FALSE(r.left.winner);
    TEST_ASSERT_TRUE(r.right.winner);
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
