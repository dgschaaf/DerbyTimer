#ifndef FINISH_CONTROLLER_H
#define FINISH_CONTROLLER_H

#include <stdint.h>

// Race result types -- exposed here so test_compute can call computeHeatResults() directly.

struct LaneResult {
	bool     foul;
	bool     dnf;
	bool     winner;
	uint32_t raceTimeUs;
	uint32_t reactionTimeUs;
	uint32_t carTimeUs;
};

struct HeatResults {
	LaneResult left;
	LaneResult right;
};

struct TimingInputs {
	uint32_t startUs;
	uint32_t leftTimeUs;
	uint32_t rightTimeUs;
	bool     leftRecorded;
	bool     rightRecorded;
	bool     leftDnf;
	bool     rightDnf;
};

// No foul: carTime = raceTime - reactionTime (gate-drop to finish)
// Foul:    carTime = raceTime + reactionTime (gate dropped before GO, car was rolling longer)
// DNF:     carTime computed but unused -- winner logic uses dnf flag, display blanks it
inline void computeHeatResults(HeatResults& result, const TimingInputs& timing) {
	result.left.dnf         = timing.leftDnf;
	result.right.dnf        = timing.rightDnf;
	result.left.raceTimeUs  = timing.leftTimeUs;
	result.right.raceTimeUs = timing.rightTimeUs;

	result.left.carTimeUs  = result.left.foul
	                       ? timing.leftTimeUs  + result.left.reactionTimeUs
	                       : timing.leftTimeUs  - result.left.reactionTimeUs;
	result.right.carTimeUs = result.right.foul
	                       ? timing.rightTimeUs + result.right.reactionTimeUs
	                       : timing.rightTimeUs - result.right.reactionTimeUs;

	bool leftValid  = !result.left.foul  && !result.left.dnf;
	bool rightValid = !result.right.foul && !result.right.dnf;
	result.left.winner  = leftValid  && (!rightValid || result.left.carTimeUs  < result.right.carTimeUs);
	result.right.winner = rightValid && (!leftValid  || result.right.carTimeUs < result.left.carTimeUs);
}

// Public API
void finishControllerSetup();
void finishControllerLoop();

#endif  // FINISH_CONTROLLER_H