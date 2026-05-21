#ifndef RACETYPES_H
#define RACETYPES_H

#include <stdint.h>

// **************** RACE DOMAIN ENUMERATIONS ****************
// These enums define the shared language of the race.
// Both controllers include this header. serialComm.h also includes it for Lane.

enum raceState : uint8_t {
	RACE_IDLE,
	RACE_STAGING,
	RACE_COUNTDOWN,
	RACE_RACING,
	RACE_COMPLETE,
	RACE_TEST
};

enum countdownState : uint8_t {
	CD_IDLE,
	CD_STAGED,
	CD_Y3,
	CD_Y2,
	CD_Y1,
	CD_GO
};

enum raceMode : uint8_t {
	MODE_GATEDROP,
	MODE_REACTION,
	MODE_PRO,
	MODE_DIALIIN,  // Set only via BLE from Race Manager — not reachable through the mode button
	MODE_COUNT     // sentinel — used for bounds checking only, not a valid mode
};

enum Lane : uint8_t {
	LANE_LEFT,
	LANE_RIGHT
};

#endif  // RACETYPES_H
