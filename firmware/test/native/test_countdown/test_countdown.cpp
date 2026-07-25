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

// ---- Stall watchdog ----
// A countdown that never reaches GO leaves the SC stuck in COUNTDOWN. The FC's
// own 10 s timeout only rescues it over a working link, so the SC needs its own.

// A healthy countdown is never flagged, at any point along the way.
void test_normal_countdown_never_stalls() {
    CountDownCtx cd;
    cd.arm(1000);

    cd.tick(MODE_GATEDROP, 1000);
    TEST_ASSERT_FALSE(cd.stalled(1000));
    cd.tick(MODE_GATEDROP, 1500);
    TEST_ASSERT_FALSE(cd.stalled(1500));
    cd.tick(MODE_GATEDROP, 2000);
    TEST_ASSERT_FALSE(cd.stalled(2000));
    cd.tick(MODE_GATEDROP, 2500);
    TEST_ASSERT_EQUAL(CD_GO, cd.state);
    TEST_ASSERT_FALSE(cd.stalled(2500));
}

// A tree stuck short of GO is flagged once the budget is exceeded. The rule is
// strictly greater-than, so the boundary itself is still healthy.
void test_stuck_countdown_stalls_past_the_budget() {
    CountDownCtx cd;
    cd.arm(1000);
    cd.tick(MODE_GATEDROP, 1000);      // advances to Y3, then the clock stops mattering

    TEST_ASSERT_FALSE(cd.stalled(6000));   // exactly at the 5000 ms budget
    TEST_ASSERT_TRUE(cd.stalled(6001));
}

// A finished countdown is not a stalled one, no matter how long the SC sits in
// CD_GO waiting for the RACING transition to commit.
void test_go_never_stalls() {
    CountDownCtx cd;
    cd.arm(1000);
    cd.state = CD_GO;
    TEST_ASSERT_FALSE(cd.stalled(999999));
}

// An unarmed countdown (between heats) must never trip the watchdog -- IDLE
// leaves the context at CD_IDLE with a stale startedAt.
void test_idle_never_stalls() {
    CountDownCtx cd;   // defaults to CD_IDLE, startedAt 0
    TEST_ASSERT_FALSE(cd.stalled(999999));
}

// arm() re-anchors the stall clock, so heat two gets a full budget rather than
// inheriting heat one's start time.
void test_arm_reanchors_the_stall_clock() {
    CountDownCtx cd;
    cd.arm(1000);
    cd.tick(MODE_GATEDROP, 1000);
    TEST_ASSERT_TRUE(cd.stalled(6001));

    cd.arm(6001);
    TEST_ASSERT_FALSE(cd.stalled(6001));
    TEST_ASSERT_FALSE(cd.stalled(11001));
    TEST_ASSERT_TRUE(cd.stalled(11002));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_full_sequence_gatedrop);
    RUN_TEST(test_full_sequence_reaction);
    RUN_TEST(test_pro_mode_skips_to_y1_at_400ms);
    RUN_TEST(test_idle_is_stable);
    RUN_TEST(test_go_is_stable);
    RUN_TEST(test_normal_countdown_never_stalls);
    RUN_TEST(test_stuck_countdown_stalls_past_the_budget);
    RUN_TEST(test_go_never_stalls);
    RUN_TEST(test_idle_never_stalls);
    RUN_TEST(test_arm_reanchors_the_stall_clock);
    return UNITY_END();
}
