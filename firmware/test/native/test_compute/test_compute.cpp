// Tests for computeHeatResults() in finishController.h.
// computeHeatResults is a pure function: TimingInputs in, HeatResults out.
// Activate: pio test -e native (requires P2-35 env setup)
#include "unity.h"
#include "arduino_mock.h"
#include "finishController.h"

void setUp() {}
void tearDown() {}

void test_no_foul_car_time_equals_race_minus_reaction() {
    TimingInputs t = {};
    t.leftTimeUs     = 5000000;
    t.leftReactionUs = 100000;
    HeatResults r = {};
    computeHeatResults(r, t);
    TEST_ASSERT_EQUAL_UINT32(4900000, r.left.carTimeUs);
}

void test_foul_car_time_equals_race_plus_reaction() {
    TimingInputs t = {};
    t.leftFoul       = true;
    t.leftTimeUs     = 5000000;
    t.leftReactionUs = 100000;
    HeatResults r = {};
    computeHeatResults(r, t);
    TEST_ASSERT_EQUAL_UINT32(5100000, r.left.carTimeUs);
    TEST_ASSERT_TRUE(r.left.foul);   // foul flag copied to the result
}

void test_both_foul_winner_is_no_result() {
    TimingInputs t = {};
    t.leftFoul = true; t.rightFoul = true;
    t.leftTimeUs = 4000000; t.rightTimeUs = 5000000;
    HeatResults r = {};
    computeHeatResults(r, t);
    TEST_ASSERT_FALSE(r.left.winner);
    TEST_ASSERT_FALSE(r.right.winner);
}

void test_one_foul_other_wins() {
    // left fouls but had faster raw time -- right still wins
    TimingInputs t = {};
    t.leftFoul = true;
    t.leftTimeUs = 3000000; t.rightTimeUs = 5000000;
    HeatResults r = {};
    computeHeatResults(r, t);
    TEST_ASSERT_FALSE(r.left.winner);
    TEST_ASSERT_TRUE(r.right.winner);
}

void test_lower_car_time_wins() {
    TimingInputs t = {};
    t.leftTimeUs      = 5000000;   // left  carTime = 4800000
    t.leftReactionUs  = 200000;
    t.rightTimeUs     = 4800000;   // right carTime = 4700000
    t.rightReactionUs = 100000;
    HeatResults r = {};
    computeHeatResults(r, t);
    TEST_ASSERT_FALSE(r.left.winner);
    TEST_ASSERT_TRUE(r.right.winner);
}

void test_equal_car_time_is_tie() {
    // Neither winner flag set; caller detects tie via winnerMask logic
    TimingInputs t = {};
    t.leftTimeUs = 5000000;  t.leftReactionUs  = 100000;
    t.rightTimeUs = 5000000; t.rightReactionUs = 100000;
    HeatResults r = {};
    computeHeatResults(r, t);
    TEST_ASSERT_FALSE(r.left.winner);
    TEST_ASSERT_FALSE(r.right.winner);
}

void test_dnf_ignored_in_winner_logic() {
    // DNF lane loses even if it would have had a faster time
    TimingInputs t = {};
    t.leftDnf    = true;
    t.leftTimeUs = 3000000;   // fast but DNF
    t.rightTimeUs = 9000000;  // slow but finished
    HeatResults r = {};
    computeHeatResults(r, t);
    TEST_ASSERT_FALSE(r.left.winner);
    TEST_ASSERT_TRUE(r.right.winner);
}

void test_reaction_exceeding_race_time_clamps_to_zero() {
    // Protocol-violation input must not wrap to ~4.29e9
    TimingInputs t = {};
    t.leftTimeUs     = 1000000;
    t.leftReactionUs = 2000000;
    HeatResults r = {};
    computeHeatResults(r, t);
    TEST_ASSERT_EQUAL_UINT32(0, r.left.carTimeUs);
}

void test_missing_reaction_car_time_equals_race_time() {
    // Reaction never arrived (lost TX): best-effort carTime = raceTime
    TimingInputs t = {};
    t.leftTimeUs = 4500000;
    HeatResults r = {};
    computeHeatResults(r, t);
    TEST_ASSERT_EQUAL_UINT32(4500000, r.left.carTimeUs);
}

void test_result_fields_copied_from_inputs() {
    TimingInputs t = {};
    t.leftTimeUs = 4000000;  t.leftReactionUs  = 250000;
    t.rightTimeUs = 4100000; t.rightReactionUs = 300000;
    t.rightDnf = true;
    HeatResults r = {};
    computeHeatResults(r, t);
    TEST_ASSERT_EQUAL_UINT32(4000000, r.left.raceTimeUs);
    TEST_ASSERT_EQUAL_UINT32(250000,  r.left.reactionTimeUs);
    TEST_ASSERT_EQUAL_UINT32(300000,  r.right.reactionTimeUs);
    TEST_ASSERT_TRUE(r.right.dnf);
    TEST_ASSERT_FALSE(r.left.dnf);
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
    RUN_TEST(test_reaction_exceeding_race_time_clamps_to_zero);
    RUN_TEST(test_missing_reaction_car_time_equals_race_time);
    RUN_TEST(test_result_fields_copied_from_inputs);
    return UNITY_END();
}
