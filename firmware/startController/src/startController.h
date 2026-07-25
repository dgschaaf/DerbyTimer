#ifndef START_CONTROLLER_H
#define START_CONTROLLER_H

#include "raceTypes.h"

// Timing data for one heat -- exposed here so test_timing can test isFoul() and reactionTimeUs().
struct raceTimingData {
	uint32_t raceStartUs;
	uint32_t laneStartUs[2];   // indexed by Lane; 0 = not triggered

	void reset() {
		raceStartUs    = 0;
		laneStartUs[0] = 0;
		laneStartUs[1] = 0;
	}
	void recordRaceStart(uint32_t tn)          { raceStartUs       = tn; }
	void recordTrigger(Lane lane, uint32_t tn) { laneStartUs[lane] = tn; }
	bool isFoul(Lane lane) const {
		if (laneStartUs[lane] == 0) return false;
		if (raceStartUs == 0)       return true;   // triggered before GO
		return (int32_t)(laneStartUs[lane] - raceStartUs) < 0;
	}
	uint32_t reactionTimeUs(Lane lane) const {
		if (isFoul(lane)) {
			// Foul with no GO timestamp (race never started): no meaningful
			// reaction duration exists -- return 0 rather than wrapping.
			if (raceStartUs == 0) return 0;
			return raceStartUs - laneStartUs[lane];
		}
		return laneStartUs[lane] - raceStartUs;
	}
};

// Drives the christmas-tree countdown timing: tick() advances
// CD_STAGED -> Y3 -> Y2 -> Y1 -> CD_GO on timed intervals (400 ms in PRO
// mode, which skips straight to Y1; 500 ms otherwise). The caller passes the
// current time into tick() -- the firmware passes millis(), tests pass a
// scripted clock -- so the sequence is a pure function of its inputs.
// handleCountdown() watches changed() to fire light updates and the GO
// actions exactly once. Reset to CD_IDLE at IDLE entry, armed at COUNTDOWN
// entry. Exposed here so test_countdown can drive the sequence.
//
// The struct also owns the stall watchdog: it knows the legal stage durations,
// and because the caller already passes the clock in, the watchdog is testable
// on the desktop.
struct CountDownCtx {
	// Longest legal countdown is 1500 ms -- CD_STAGED advances immediately,
	// then three 500 ms stages; PRO reaches GO in 400 ms. A tree still short of
	// GO well past that is not going to arrive. The bound is deliberately
	// generous, and half the FC's 10 s countdown timeout, so the SC (which
	// owns COUNTDOWN transitions) aborts first and the FC follows the state
	// message rather than both controllers timing out independently.
	static const unsigned long stallTimeoutMs = 5000;

	countdownState state     = CD_IDLE;
	countdownState prev      = CD_IDLE;
	unsigned long  timer     = 0;
	unsigned long  delay     = 0;
	unsigned long  startedAt = 0;   // when this countdown was armed; stall reference

	bool changed() const { return state != prev; }

	// Begin a countdown and start the stall clock.
	void arm(unsigned long now) {
		state     = CD_STAGED;
		startedAt = now;
	}

	// True once the tree has failed to reach GO within its legal budget.
	// A finished (CD_GO) or unarmed (CD_IDLE) countdown can never be stalled.
	bool stalled(unsigned long now) const {
		if (state == CD_IDLE || state == CD_GO) return false;
		return (now - startedAt) > stallTimeoutMs;
	}

	void tick(raceMode mode, unsigned long now) {
		prev = state;
		switch (state) {
			case CD_STAGED:
				delay = (mode == MODE_PRO) ? 400 : 500;
				state = (mode == MODE_PRO) ? CD_Y1 : CD_Y3;
				timer = now;
				break;
			case CD_Y3:
				if (now - timer >= delay) { state = CD_Y2; timer = now; }
				break;
			case CD_Y2:
				if (now - timer >= delay) { state = CD_Y1; timer = now; }
				break;
			case CD_Y1:
				if (now - timer >= delay) { state = CD_GO; timer = now; }
				break;
			default: break;
		}
	}
};

// Public API
void startControllerSetup();
void startControllerLoop();

#endif  // START_CONTROLLER_H