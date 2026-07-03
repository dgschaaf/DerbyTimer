// Tests for CountDownCtx::tick() in startController.h -- the christmas-tree
// countdown sequencing (CD_STAGED -> Y3 -> Y2 -> Y1 -> CD_GO). tick() takes
// the current time as an argument, so the whole heat-fairness timing runs as
// a table of (elapsed, expected state) rows with no real sleeps.
// Activate: pio test -e native
#include "unity.h"
#include "arduino_mock.h"
#include "startController.h"

void setUp() {}
void tearDown() {}

// Drives a full non-PRO countdown (500 ms steps) from STAGED to GO, asserting
// each step advances only when the delay has elapsed and the timer re-anchors.
// Used for both GATEDROP and REACTION, which share the 500 ms / Y3-first path.
static void assert_500ms_sequence(raceMode mode) {
    CountDownCtx cd;
    cd.state = CD_STAGED;

    // STAGED always advances on the first tick (arms delay + timer), no wait.
    cd.tick(mode, 1000);
    TEST_ASSERT_EQUAL(CD_Y3, cd.state);
    TEST_ASSERT_TRUE(cd.changed());

    // Y3 holds until 500 ms elapse from the step's timer (1000).
    cd.tick(mode, 1499);
    TEST_ASSERT_EQUAL(CD_Y3, cd.state);
    TEST_ASSERT_FALSE(cd.changed());

    // Boundary: elapsed == delay advances (rule is >=).
    cd.tick(mode, 1500);
    TEST_ASSERT_EQUAL(CD_Y2, cd.state);
    TEST_ASSERT_TRUE(cd.changed());

    // Timer re-anchored at 1500, so Y2 needs 2000, not 2000-from-start-1000.
    cd.tick(mode, 1999);
    TEST_ASSERT_EQUAL(CD_Y2, cd.state);
    cd.tick(mode, 2000);
    TEST_ASSERT_EQUAL(CD_Y1, cd.state);

    // Y1 -> GO one more 500 ms step later.
    cd.tick(mode, 2499);
    TEST_ASSERT_EQUAL(CD_Y1, cd.state);
    cd.tick(mode, 2500);
    TEST_ASSERT_EQUAL(CD_GO, cd.state);
    TEST_ASSERT_TRUE(cd.changed());
}

void test_full_sequence_gatedrop() {
    assert_500ms_sequence(MODE_GATEDROP);
}

void test_full_sequence_reaction() {
    assert_500ms_sequence(MODE_REACTION);
}

void test_pro_mode_skips_to_y1_at_400ms() {
    CountDownCtx cd;
    cd.state = CD_STAGED;

    // PRO skips Y3/Y2 entirely: STAGED goes straight to Y1 with a 400 ms step.
    cd.tick(MODE_PRO, 1000);
    TEST_ASSERT_EQUAL(CD_Y1, cd.state);
    TEST_ASSERT_TRUE(cd.changed());

    cd.tick(MODE_PRO, 1399);
    TEST_ASSERT_EQUAL(CD_Y1, cd.state);
    TEST_ASSERT_FALSE(cd.changed());

    cd.tick(MODE_PRO, 1400);
    TEST_ASSERT_EQUAL(CD_GO, cd.state);
    TEST_ASSERT_TRUE(cd.changed());
}

void test_idle_is_stable() {
    CountDownCtx cd;   // defaults to CD_IDLE
    cd.tick(MODE_GATEDROP, 1000);
    TEST_ASSERT_EQUAL(CD_IDLE, cd.state);
    TEST_ASSERT_FALSE(cd.changed());
}

void test_go_is_stable() {
    CountDownCtx cd;
    cd.state = CD_GO;
    cd.tick(MODE_GATEDROP, 1000);
    TEST_ASSERT_EQUAL(CD_GO, cd.state);
    TEST_ASSERT_FALSE(cd.changed());
    cd.tick(MODE_GATEDROP, 99999);
    TEST_ASSERT_EQUAL(CD_GO, cd.state);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_full_sequence_gatedrop);
    RUN_TEST(test_full_sequence_reaction);
    RUN_TEST(test_pro_mode_skips_to_y1_at_400ms);
    RUN_TEST(test_idle_is_stable);
    RUN_TEST(test_go_is_stable);
    return UNITY_END();
}
