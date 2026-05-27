// Tests for raceTimingData::isFoul() and raceTimingData::reactionTimeUs()
// in startController.h.
// Activate: pio test -e native (requires P2-35 env setup)
#include "unity.h"
#include "arduino_mock.h"
#include "startController.h"

void setUp() {}
void tearDown() {}

void test_foul_trigger_before_race_start() {
    raceTimingData r;
    r.reset();
    r.recordRaceStart(1000);
    r.recordTrigger(LANE_LEFT, 500);
    TEST_ASSERT_TRUE(r.isFoul(LANE_LEFT));
}

void test_foul_trigger_after_race_start() {
    raceTimingData r;
    r.reset();
    r.recordRaceStart(1000);
    r.recordTrigger(LANE_LEFT, 1500);
    TEST_ASSERT_FALSE(r.isFoul(LANE_LEFT));
}

void test_foul_trigger_exact_boundary_not_foul() {
    // delta = 0 is not < 0, so not a foul
    raceTimingData r;
    r.reset();
    r.recordRaceStart(1000);
    r.recordTrigger(LANE_LEFT, 1000);
    TEST_ASSERT_FALSE(r.isFoul(LANE_LEFT));
}

void test_reaction_early_trigger_positive() {
    // foul: reactionTimeUs = raceStart - laneStart
    raceTimingData r;
    r.reset();
    r.recordRaceStart(1000);
    r.recordTrigger(LANE_LEFT, 500);
    TEST_ASSERT_EQUAL_UINT32(500, r.reactionTimeUs(LANE_LEFT));
}

void test_reaction_late_trigger_positive() {
    // normal: reactionTimeUs = laneStart - raceStart
    raceTimingData r;
    r.reset();
    r.recordRaceStart(1000);
    r.recordTrigger(LANE_LEFT, 1300);
    TEST_ASSERT_EQUAL_UINT32(300, r.reactionTimeUs(LANE_LEFT));
}

void test_reaction_zero_delta() {
    raceTimingData r;
    r.reset();
    r.recordRaceStart(1000);
    r.recordTrigger(LANE_LEFT, 1000);
    TEST_ASSERT_EQUAL_UINT32(0, r.reactionTimeUs(LANE_LEFT));
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
