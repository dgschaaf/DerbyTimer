// Tests for bitmask encoding in serialComm.h:
// foul bits, winner bits, race-start bits.
// Activate: pio test -e native (requires P2-35 env setup)
#include "unity.h"
#include "arduino_mock.h"
#include "serialComm.h"

void setUp() {}
void tearDown() {}

void test_foul_left_or_right_equals_both() {
    TEST_ASSERT_EQUAL(foul_both, foul_left | foul_right);
}

void test_foul_left_and_right_no_overlap() {
    // foul_left and foul_right must occupy different bits — same byte, independent flags
    TEST_ASSERT_EQUAL(0, foul_left & foul_right);
}

void test_winner_left_and_right_are_distinct() {
    TEST_ASSERT_NOT_EQUAL(winner_leftWin, winner_rightWin);
    TEST_ASSERT_EQUAL(0, winner_leftWin & winner_rightWin);
}

void test_winner_tie_is_distinct_from_left_and_right() {
    TEST_ASSERT_EQUAL(0, winner_tie & winner_leftWin);
    TEST_ASSERT_EQUAL(0, winner_tie & winner_rightWin);
}

void test_winner_no_result_is_distinct() {
    TEST_ASSERT_EQUAL(0, winner_noResult & winner_leftWin);
    TEST_ASSERT_EQUAL(0, winner_noResult & winner_rightWin);
    TEST_ASSERT_EQUAL(0, winner_noResult & winner_tie);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_foul_left_or_right_equals_both);
    RUN_TEST(test_foul_left_and_right_no_overlap);
    RUN_TEST(test_winner_left_and_right_are_distinct);
    RUN_TEST(test_winner_tie_is_distinct_from_left_and_right);
    RUN_TEST(test_winner_no_result_is_distinct);
    return UNITY_END();
}
