// Tests for buildLightConfig() and blink state machine in lights.cpp.
// Activate: pio test -e native (requires P2-35 env setup and P2-12 extraction)
#include "unity.h"
#include "arduino_mock.h"

void setUp() {}
void tearDown() {}

void test_build_light_config_cd_y1_non_pro() {
    // CD_Y1 event in non-PRO mode -> LIGHT_Y1 only (single yellow)
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (expose buildLightConfig)");
}

void test_build_light_config_cd_y1_pro_mode() {
    // CD_Y1 event in PRO mode -> LIGHT_Y3|Y2|Y1 (all three yellows lit)
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (expose buildLightConfig)");
}

void test_build_light_config_foul_overlay() {
    // Foul active -> foul lights (LIGHT_FL / LIGHT_FR) added to pattern
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (expose buildLightConfig)");
}

void test_build_light_config_blue_always_included() {
    // Blue status lights always present regardless of event or mode
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (expose buildLightConfig)");
}

void test_update_blink_toggles_at_correct_rate() {
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (expose updateBlink)");
}

void test_blink_total_count_is_2x_count() {
    // count=3 -> 6 total edge transitions (on/off/on/off/on/off)
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (expose updateBlink)");
}

void test_blink_final_pattern_applied_on_completion() {
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (expose updateBlink)");
}

void test_cancel_blink_stops_without_final_pattern() {
    TEST_IGNORE_MESSAGE("Stub -- implement after P2-12 (expose cancelBlink)");
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_build_light_config_cd_y1_non_pro);
    RUN_TEST(test_build_light_config_cd_y1_pro_mode);
    RUN_TEST(test_build_light_config_foul_overlay);
    RUN_TEST(test_build_light_config_blue_always_included);
    RUN_TEST(test_update_blink_toggles_at_correct_rate);
    RUN_TEST(test_blink_total_count_is_2x_count);
    RUN_TEST(test_blink_final_pattern_applied_on_completion);
    RUN_TEST(test_cancel_blink_stops_without_final_pattern);
    return UNITY_END();
}
