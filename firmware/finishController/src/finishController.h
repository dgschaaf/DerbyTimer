#ifndef FINISH_CONTROLLER_H
#define FINISH_CONTROLLER_H

#include <stdint.h>

// Race result types -- exposed here so test_compute can call computeHeatResults() directly.

// Final computed outcome for one lane: copied flags (foul/dnf), the raw
// race time (GO to sensor), the reaction time received from the SC, and the
// derived carTimeUs used for winner determination and display.
struct LaneResult {
	bool     foul;
	bool     dnf;
	bool     winner;
	uint32_t raceTimeUs;
	uint32_t reactionTimeUs;
	uint32_t carTimeUs;
};

// Both lanes' outcomes for one heat. Pure OUTPUT of computeHeatResults();
// reset to {} at IDLE entry and COMPLETE exit.
struct HeatResults {
	LaneResult left;
	LaneResult right;
};

// Everything computeHeatResults() needs, accumulated during the heat:
// sensor times/DNFs from handleSensors(), foul flags and reaction times
// from handleRxReaction(). Pure INPUT -- reset to {} at IDLE entry and
// COMPLETE exit; per-heat fields re-cleared at RACING entry.
struct TimingInputs {
	uint32_t startUs;
	uint32_t leftTimeUs;
	uint32_t rightTimeUs;
	bool     leftRecorded;
	bool     rightRecorded;
	bool     leftDnf;
	bool     rightDnf;
	bool     leftFoul;
	bool     rightFoul;
	uint32_t leftReactionUs;
	uint32_t rightReactionUs;
};

// Pure function: TimingInputs in, HeatResults out (no hidden state on either side).
// No foul: carTime = raceTime - reactionTime (gate-drop to finish)
// Foul:    carTime = raceTime + reactionTime (gate dropped before GO, car was rolling longer)
// DNF:     carTime computed but unused -- winner logic uses dnf flag, display blanks it
inline void computeHeatResults(HeatResults& result, const TimingInputs& timing) {
	result.left.foul            = timing.leftFoul;
	result.right.foul           = timing.rightFoul;
	result.left.dnf             = timing.leftDnf;
	result.right.dnf            = timing.rightDnf;
	result.left.raceTimeUs      = timing.leftTimeUs;
	result.right.raceTimeUs     = timing.rightTimeUs;
	result.left.reactionTimeUs  = timing.leftReactionUs;
	result.right.reactionTimeUs = timing.rightReactionUs;

	// Clamp the no-foul subtraction at 0: a reaction time exceeding the race
	// time is a protocol violation, and wrapping would fabricate a huge time.
	result.left.carTimeUs  = timing.leftFoul
	                       ? timing.leftTimeUs  + timing.leftReactionUs
	                       : (timing.leftReactionUs  > timing.leftTimeUs  ? 0
	                          : timing.leftTimeUs  - timing.leftReactionUs);
	result.right.carTimeUs = timing.rightFoul
	                       ? timing.rightTimeUs + timing.rightReactionUs
	                       : (timing.rightReactionUs > timing.rightTimeUs ? 0
	                          : timing.rightTimeUs - timing.rightReactionUs);

	bool leftValid  = !timing.leftFoul  && !timing.leftDnf;
	bool rightValid = !timing.rightFoul && !timing.rightDnf;
	result.left.winner  = leftValid  && (!rightValid || result.left.carTimeUs  < result.right.carTimeUs);
	result.right.winner = rightValid && (!leftValid  || result.right.carTimeUs < result.left.carTimeUs);
}

// Public API
void finishControllerSetup();
void finishControllerLoop();

#endif  // FINISH_CONTROLLER_H