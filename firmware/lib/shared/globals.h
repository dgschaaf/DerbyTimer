#ifndef GLOBALS_H
#define GLOBALS_H

// **************** BITMASKS & DEFINITIONS ****************
// Winner event codes
#define winner_leftWin	0b0001
#define winner_rightWin	0b0010
#define winner_tie		0b0100
// Start event codes
#define start_race		0b0001
#define start_left		0b0010
#define start_right		0b0100
#define start_all		0b0111
// Foul event codes
#define foul_left		0b0001
#define foul_right		0b0010
#define foul_both		0b0011

// **************** ENUMERATIONS ****************
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
	MODE_DIALIIN 
	};
#endif