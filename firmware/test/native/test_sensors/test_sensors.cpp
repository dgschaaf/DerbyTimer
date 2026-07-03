// Tests for the finish-line beam-break capture rules (sensors.cpp). The real
// sensors.cpp is compiled into this translation unit against the mock
// Arduino.h, so the file-static state and the file-static onBeamBreak() are
// visible here. onBeamBreak() takes the timestamp as an argument (the ISRs
// pass micros(), we pass a controlled value), which is what lets the capture
// logic -- normally reachable only from an untestable ISR -- run on the desktop.
// Activate: pio test -e native
#include "unity.h"
#include "arduino_mock.h"
#include "raceTypes.h"
#include "../../../finishController/src/sensors.cpp"

// config.minRaceTimeUs is 500000 (0.5 s) -- the power-up transient filter.
static const uint32_t T0      = 1000000;   // an arbitrary race-start stamp
static const uint32_t MIN_US  = 500000;

// Every test starts disarmed and cleared. disarmSensors() sets armed=false and
// clears the latch and finished flags; tests that need a live race call
// armSensors() themselves.
void setUp()    { disarmSensors(); }
void tearDown() {}

void test_not_armed_ignores_trigger() {
    // No armSensors() -- a beam break before the race must do nothing.
    onBeamBreak(LANE_LEFT, T0 + 2000000);
    TEST_ASSERT_FALSE(isLeftFinished());
}

void test_trigger_inside_min_race_time_ignored() {
    armSensors(T0);
    onBeamBreak(LANE_LEFT, T0 + 400000);   // 0.4 s elapsed, inside the filter
    TEST_ASSERT_FALSE(isLeftFinished());
}

void test_trigger_exactly_at_min_race_time_ignored() {
    // Rule is strictly greater-than, so the boundary itself is still filtered.
    armSensors(T0);
    onBeamBreak(LANE_LEFT, T0 + MIN_US);
    TEST_ASSERT_FALSE(isLeftFinished());
}

void test_valid_trigger_latches_time_and_flag() {
    armSensors(T0);
    onBeamBreak(LANE_LEFT, T0 + 2500000);
    TEST_ASSERT_TRUE(isLeftFinished());
    TEST_ASSERT_EQUAL_UINT32(2500000, getLeftTimeUs());
}

void test_second_trigger_same_lane_ignored() {
    armSensors(T0);
    onBeamBreak(LANE_LEFT, T0 + 2000000);   // latches 2.0 s
    onBeamBreak(LANE_LEFT, T0 + 3000000);   // later beam break must not overwrite
    TEST_ASSERT_EQUAL_UINT32(2000000, getLeftTimeUs());
}

void test_lanes_are_independent() {
    armSensors(T0);
    onBeamBreak(LANE_LEFT, T0 + 2000000);
    TEST_ASSERT_TRUE(isLeftFinished());
    TEST_ASSERT_FALSE(isRightFinished());   // left finishing does not finish right

    onBeamBreak(LANE_RIGHT, T0 + 3000000);
    TEST_ASSERT_TRUE(isRightFinished());
    TEST_ASSERT_EQUAL_UINT32(2000000, getLeftTimeUs());
    TEST_ASSERT_EQUAL_UINT32(3000000, getRightTimeUs());
}

void test_disarm_clears_finished_flags() {
    armSensors(T0);
    onBeamBreak(LANE_LEFT, T0 + 2000000);
    TEST_ASSERT_TRUE(isLeftFinished());

    disarmSensors();
    TEST_ASSERT_FALSE(isLeftFinished());
    TEST_ASSERT_FALSE(isRightFinished());
}

void test_rearm_resets_latch_and_flags() {
    armSensors(T0);
    onBeamBreak(LANE_LEFT, T0 + 2000000);
    TEST_ASSERT_TRUE(isLeftFinished());

    // A new heat: re-arm with a fresh start stamp clears the previous result
    // and re-opens the latch so the next valid break is captured.
    uint32_t t0b = 5000000;
    armSensors(t0b);
    TEST_ASSERT_FALSE(isLeftFinished());
    TEST_ASSERT_FALSE(isRightFinished());

    onBeamBreak(LANE_LEFT, t0b + 1000000);
    TEST_ASSERT_TRUE(isLeftFinished());
    TEST_ASSERT_EQUAL_UINT32(1000000, getLeftTimeUs());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_not_armed_ignores_trigger);
    RUN_TEST(test_trigger_inside_min_race_time_ignored);
    RUN_TEST(test_trigger_exactly_at_min_race_time_ignored);
    RUN_TEST(test_valid_trigger_latches_time_and_flag);
    RUN_TEST(test_second_trigger_same_lane_ignored);
    RUN_TEST(test_lanes_are_independent);
    RUN_TEST(test_disarm_clears_finished_flags);
    RUN_TEST(test_rearm_resets_latch_and_flags);
    return UNITY_END();
}
