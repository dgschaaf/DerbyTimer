#ifndef GLOBALS_H
#define GLOBALS_H

// **************** BITMASKS & DEFINITIONS ****************


// **************** ENUMERATIONS ****************
enum raceState uint8_t { 
	RACE_IDLE,
	RACE_STAGING,
	RACE_COUNTDOWN,
	RACE_RACING, 
	RACE_COMPLETE,
	RACE_TEST
	};

enum raceMode uint8_t {
	MODE_GATEDROP,
	MODE_REACTION,
	MODE_PRO,
	MODE_DIALIIN 
	};
};
	

// **************** Global Race Variables ****************
extern unsigned long leftReactionTime;	// Reaction time for left lane (us)
extern unsigned long rightReactionTime;	// Reaction time for right lane (us)

extern unsigned long raceStartTime;		// Start time of current race (us)
extern unsigned long leftStartTime;		// Start time for left lane (us)
extern unsigned long rightStartTime;	// Start time for right lane (us)

extern bool leftFoul;					// Foul status of left lane
extern bool rightFoul;					// Foul status of right lane

extern raceState targetState;			// Requested next state
extern raceState currentState;			// Current state

extern raceMode targetMode;				// Requested next race mode
extern raceMode currentMode;			// Current race mode

#endif