// Tests for us->ms conversion and BCD digit extraction in display.cpp.
// Activate: pio test -e native (requires P2-35 env setup)
#include "unity.h"
#include "arduino_mock.h"

void setUp() {}
void tearDown() {}

// us->ms conversion with round-half-up and 99998 clamp
void test_us_to_ms_rounds_half_up() {
    // 1,234,567 us -> 1235 ms  (567 us rounds up)
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (expose usToMs helper)");
}

void test_us_to_ms_rounds_down_below_half() {
    // 499 us -> 0 ms
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (expose usToMs helper)");
}

void test_us_to_ms_rounds_up_at_half() {
    // 500 us -> 1 ms
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (expose usToMs helper)");
}

void test_us_to_ms_clamp_near_max() {
    // 99,998,500 us -> 99998 ms (clamp to max displayable)
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (expose usToMs helper)");
}

void test_us_to_ms_clamp_over_max() {
    // 99,999,000 us -> 99998 ms (clamp)
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (expose usToMs helper)");
}

// BCD digit extraction: 5-digit array [ten-thousands, thousands, hundreds, tens, ones]
void test_bcd_typical_value() {
    // 12345 ms -> [1, 2, 3, 4, 5]
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (expose bcdDigits helper)");
}

void test_bcd_leading_zeros_preserved() {
    // 5 ms -> [0, 0, 0, 0, 5]
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (expose bcdDigits helper)");
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_us_to_ms_rounds_half_up);
    RUN_TEST(test_us_to_ms_rounds_down_below_half);
    RUN_TEST(test_us_to_ms_rounds_up_at_half);
    RUN_TEST(test_us_to_ms_clamp_near_max);
    RUN_TEST(test_us_to_ms_clamp_over_max);
    RUN_TEST(test_bcd_typical_value);
    RUN_TEST(test_bcd_leading_zeros_preserved);
    return UNITY_END();
}
