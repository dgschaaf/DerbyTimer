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
// actions exactly once. Reset to CD_IDLE at IDLE entry, to CD_STAGED at
// COUNTDOWN entry. Exposed here so test_countdown can drive the sequence.
struct CountDownCtx {
	countdownState state  = CD_IDLE;
	countdownState prev   = CD_IDLE;
	unsigned long  timer  = 0;
	unsigned long  delay  = 0;

	bool changed() const { return state != prev; }

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