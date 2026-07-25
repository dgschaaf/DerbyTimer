#ifndef BUTTONS_H
#define BUTTONS_H

/**
 * @brief Operator buttons and lane triggers for the Start Controller.
 *
 * Left is D18, Right is D19, Start is A6, Mode is A7.
 *
 * DEBOUNCE IS HARDWARE. All four inputs pass through Schmitt triggers on the
 * PCB shield, so the levels this module reads are already clean. Nothing here
 * debounces, and nothing here should start: the analog re-read interval below
 * is a cost optimization (ADC reads are slow), not a settling filter, and the
 * click detector keys off a single clean edge for the same reason.
 *
 * Two ways to read a button, and the choice is a policy, not a preference:
 * the operator controls (Start, Mode) act on the RELEASE edge - a completed
 * click - while the lane triggers act on the PRESS, because reaction timing
 * needs the instant the racer's hand moved, not the instant it let go.
 */

// Release-edge detector for one button. Kept header-only and free of any
// Arduino dependency so the rule can be tested on the desktop directly.
struct ClickDetector {
	bool prev = false;

	// Feed the current level once per pass; true exactly on the release edge.
	bool update(bool pressed) {
		bool released = prev && !pressed;
		prev = pressed;
		return released;
	}
};

// Setup/teardown
void setupButtons();

// Level readers -- what the button is doing right now. Correct for the lane
// triggers, for the hold-MODE-at-boot self-test entry, and for the RACE_TEST
// button checks (which verify a button electrically works). Using these for an
// operator action is what let a single held button fire in two states.
bool isStartPressed();
bool isModePressed();
bool isLeftPressed();
bool isRightPressed();

// Click layer. updateButtons() samples both operator buttons once per loop
// pass; the accessors report whether a click completed IN THAT PASS.
//
// Deliberately not latched: an unread click is gone, not stored. A stored
// click would recreate the bug this layer removes -- a Mode click during
// RACING sitting in a buffer until IDLE picks it up and advances the mode.
void updateButtons();
bool startClicked();
bool modeClicked();

#endif  // BUTTONS_H
