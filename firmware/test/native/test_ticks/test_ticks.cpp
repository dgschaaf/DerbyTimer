// Tests for the two time-driven Start Controller animations now that their
// tick functions take the current time as an argument: the christmas-tree
// blink (lights.cpp) and the gate return pulse (gates.cpp). Both .cpp files
// are compiled into this translation unit so their state (blinkState global,
// gs file-static) is inspectable. mockLastShiftOut (in the Arduino mock)
// records the last light pattern actually pushed to the shift register.
// Activate: pio test -e native
#include "unity.h"
#include "arduino_mock.h"
#include "raceTypes.h"
#include "../../../startController/src/lights.cpp"
#include "../../../startController/src/gates.cpp"

// Each test starts from a clean animation + gate state.
void setUp() {
    blinkState.active = false;
    gs = {false, false, false, 0};
    mockLastShiftOut = 0;
    mockMillis = 0;
}
void tearDown() {}

// ---- Blink animation ----

void test_blink_applies_pattern1_at_start() {
    mockMillis = 1000;
    startBlink(LIGHT_Y1, LIGHT_OFF, 3, 250, LIGHT_GO);
    TEST_ASSERT_TRUE(isBlinking());
    TEST_ASSERT_EQUAL_UINT8(LIGHT_Y1, mockLastShiftOut);   // pattern1 shown immediately
}

void test_blink_holds_until_rate_elapses() {
    mockMillis = 1000;
    startBlink(LIGHT_Y1, LIGHT_OFF, 3, 250, LIGHT_GO);

    TEST_ASSERT_TRUE(updateBlink(1000 + 249));             // one tick short of the rate
    TEST_ASSERT_EQUAL_UINT8(LIGHT_Y1, mockLastShiftOut);   // no toggle yet

    TEST_ASSERT_TRUE(updateBlink(1000 + 250));             // boundary: toggles now
    TEST_ASSERT_EQUAL_UINT8(LIGHT_OFF, mockLastShiftOut);  // pattern2 shown
}

void test_blink_runs_count_times_then_final_pattern() {
    const uint8_t count = 3;
    mockMillis = 0;
    startBlink(LIGHT_Y1, LIGHT_OFF, count, 250, LIGHT_GO);

    // Each blink is two half-cycles, so count*2 toggles complete the animation.
    unsigned long t = 0;
    int toggles = 0;
    bool running = true;
    while (running && toggles < 100) {   // cap guards against a stuck loop
        t += 250;
        running = updateBlink(t);
        toggles++;
    }

    TEST_ASSERT_EQUAL_INT(count * 2, toggles);             // finished on the last half-cycle
    TEST_ASSERT_FALSE(isBlinking());
    TEST_ASSERT_EQUAL_UINT8(LIGHT_GO, mockLastShiftOut);   // final pattern applied
}

void test_blink_count_zero_applies_final_immediately() {
    mockMillis = 500;
    startBlink(LIGHT_Y1, LIGHT_OFF, 0, 250, LIGHT_GO);
    TEST_ASSERT_FALSE(isBlinking());
    TEST_ASSERT_EQUAL_UINT8(LIGHT_GO, mockLastShiftOut);   // no blinking, straight to final
}

void test_cancel_blink_stops_animation() {
    mockMillis = 0;
    startBlink(LIGHT_Y1, LIGHT_OFF, 3, 250, LIGHT_GO);
    TEST_ASSERT_TRUE(isBlinking());

    cancelBlink();
    TEST_ASSERT_FALSE(isBlinking());
    TEST_ASSERT_FALSE(updateBlink(99999));                 // inactive: nothing further happens
}

// ---- Gate return sequence ----

void test_gate_return_ready_only_after_wait() {
    mockMillis = 1000;
    returnGates();                          // energize magnets; return pulse begins
    TEST_ASSERT_TRUE(isLaneUp(LANE_LEFT));
    TEST_ASSERT_TRUE(isLaneUp(LANE_RIGHT));
    TEST_ASSERT_FALSE(areLanesReady());     // not ready while the pulse is active

    updateGates(1000 + 499);
    TEST_ASSERT_FALSE(areLanesReady());     // still pulsing one tick short of 500 ms

    updateGates(1000 + 500);                // boundary: pulse ends
    TEST_ASSERT_TRUE(areLanesReady());      // both gates up, pulse done
}

void test_dropgate_is_idempotent_and_per_lane() {
    mockMillis = 0;
    returnGates();
    updateGates(1000);                      // complete the return so both are up
    TEST_ASSERT_TRUE(areLanesReady());

    dropGate(LANE_LEFT);
    TEST_ASSERT_FALSE(isLaneUp(LANE_LEFT));
    TEST_ASSERT_TRUE(isLaneUp(LANE_RIGHT));  // right lane unaffected

    dropGate(LANE_LEFT);                     // idempotent: dropping again is a no-op
    TEST_ASSERT_FALSE(isLaneUp(LANE_LEFT));

    dropGate(LANE_RIGHT);
    TEST_ASSERT_FALSE(isLaneUp(LANE_RIGHT));
    TEST_ASSERT_FALSE(areLanesReady());      // no lanes up
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_blink_applies_pattern1_at_start);
    RUN_TEST(test_blink_holds_until_rate_elapses);
    RUN_TEST(test_blink_runs_count_times_then_final_pattern);
    RUN_TEST(test_blink_count_zero_applies_final_immediately);
    RUN_TEST(test_cancel_blink_stops_animation);
    RUN_TEST(test_gate_return_ready_only_after_wait);
    RUN_TEST(test_dropgate_is_idempotent_and_per_lane);
    return UNITY_END();
}
