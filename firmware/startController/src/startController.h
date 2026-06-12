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

// Public API
void startControllerSetup();
void startControllerLoop();

#endif  // START_CONTROLLER_H