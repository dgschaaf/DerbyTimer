// Tests for extractDisplayDigits() in display.h -- the pure us -> 5-digit
// conversion behind updateDisplay(). Display format is SS.mmm.
// Activate: pio test -e native
#include "unity.h"
#include "arduino_mock.h"
#include "display.h"

void setUp() {}
void tearDown() {}

static void assertDigits(uint32_t timeUs, uint8_t e0, uint8_t e1, uint8_t e2, uint8_t e3, uint8_t e4) {
    uint8_t d[DISPLAY_NUM_DIGITS];
    extractDisplayDigits(timeUs, d);
    TEST_ASSERT_EQUAL_UINT8(e0, d[0]);
    TEST_ASSERT_EQUAL_UINT8(e1, d[1]);
    TEST_ASSERT_EQUAL_UINT8(e2, d[2]);
    TEST_ASSERT_EQUAL_UINT8(e3, d[3]);
    TEST_ASSERT_EQUAL_UINT8(e4, d[4]);
}

void test_zero_shows_all_zeros() {
    assertDigits(0, 0, 0, 0, 0, 0);
}

void test_typical_race_time() {
    // 3.456789 s -> 03.457 (rounded up)
    assertDigits(3456789, 0, 3, 4, 5, 7);
}

void test_rounding_down_below_half_ms() {
    // 1234499 us -> 1234.499 ms -> rounds to 1234 -> 01.234
    assertDigits(1234499, 0, 1, 2, 3, 4);
}

void test_rounding_up_at_half_ms() {
    // 1234500 us -> rounds to 1235 -> 01.235
    assertDigits(1234500, 0, 1, 2, 3, 5);
}

void test_error_sentinel_88888() {
    // 88888000 us -> 88.888 (RACE_TEST all-segments pattern)
    assertDigits(88888000, 8, 8, 8, 8, 8);
}

void test_clamp_at_display_max() {
    // Anything that rounds past 99998 ms clamps to 99.998
    assertDigits(99999000, 9, 9, 9, 9, 8);
    assertDigits(0xFFFFFFFF, 9, 9, 9, 9, 8);
}

void test_just_below_clamp_not_clamped() {
    // 99997499 us -> rounds to 99997 -> 99.997
    assertDigits(99997499, 9, 9, 9, 9, 7);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_shows_all_zeros);
    RUN_TEST(test_typical_race_time);
    RUN_TEST(test_rounding_down_below_half_ms);
    RUN_TEST(test_rounding_up_at_half_ms);
    RUN_TEST(test_error_sentinel_88888);
    RUN_TEST(test_clamp_at_display_max);
    RUN_TEST(test_just_below_clamp_not_clamped);
    return UNITY_END();
}
