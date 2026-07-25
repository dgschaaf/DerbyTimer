// Tests for ClickDetector in buttons.h -- the operator-button edge rule.
// Start and Mode act on the RELEASE edge, so a button still held across a
// state change cannot fire an action in the state it lands in. The detector
// takes the button level as an argument, so the rule tests as a sequence of
// levels with no Arduino involved at all.
// Activate: pio test -e native
#include "unity.h"
#include "arduino_mock.h"
#include "buttons.h"

void setUp() {}
void tearDown() {}

// An untouched button never reports a click, however long it is watched.
void test_idle_button_never_clicks() {
    ClickDetector d;
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_FALSE(d.update(false));
    }
}

// The core of the policy: pressing is not the action, releasing is.
void test_click_fires_on_release_not_on_press() {
    ClickDetector d;
    TEST_ASSERT_FALSE(d.update(true));    // going down does nothing
    TEST_ASSERT_TRUE(d.update(false));    // coming up is the click
}

// A held button reports exactly one click, on release -- never a repeat while
// held. This is what stops one long press from acting twice.
void test_hold_reports_one_click_on_release() {
    ClickDetector d;
    d.update(true);
    for (int i = 0; i < 20; i++) {
        TEST_ASSERT_FALSE(d.update(true));
    }
    TEST_ASSERT_TRUE(d.update(false));
    TEST_ASSERT_FALSE(d.update(false));
}

// A release with no press ahead of it is not a click. This is the case that
// matters at a state change: a handler that starts watching a button already
// on its way up must not treat that as an action.
void test_release_without_a_press_is_not_a_click() {
    ClickDetector d;
    TEST_ASSERT_FALSE(d.update(false));
}

// Consecutive clicks are independent -- the detector re-arms itself.
void test_second_click_is_reported() {
    ClickDetector d;
    d.update(true);
    TEST_ASSERT_TRUE(d.update(false));
    d.update(true);
    TEST_ASSERT_TRUE(d.update(false));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_idle_button_never_clicks);
    RUN_TEST(test_click_fires_on_release_not_on_press);
    RUN_TEST(test_hold_reports_one_click_on_release);
    RUN_TEST(test_release_without_a_press_is_not_a_click);
    RUN_TEST(test_second_click_is_reported);
    return UNITY_END();
}
