#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include "raceTypes.h"
#include "serialComm.h"

// ---------------------------------------------------------------
// State Machine Struct
// ---------------------------------------------------------------
// Shared by both controllers. Manages the allowed transition table,
// initiator-driven (selfTransition) and follower-driven (rxTransition)
// state changes, and entry/exit flag semantics.
//
// Coordination model (who initiates each transition):
//   IDLE      -> STAGING     : SC initiates, FC follows
//   STAGING   -> IDLE        : SC initiates, FC follows
//   STAGING   -> COUNTDOWN   : SC initiates, FC follows
//   COUNTDOWN -> RACING      : SC initiates, FC follows
//   RACING    -> COMPLETE    : FC initiates, SC follows
//   COMPLETE  -> IDLE        : FC initiates, SC follows
// ---------------------------------------------------------------

struct stateMachine {
	raceState current;
	raceState target;
	bool entry;
	bool exit;

	bool allowedTransition(raceState next) {
		// Allowed transitions table (FROM x TO)
		static constexpr bool allowed[6][6] = {
		/* FROM\TO:  IDLE STAG CNTD RACE CMPL TEST */
		/*IDLE*/     {0,   1,   0,   0,   0,   1},
		/*STAGING*/  {0,   0,   1,   0,   0,   0},
		/*COUNTDOWN*/{0,   0,   0,   1,   0,   0},
		/*RACING*/   {0,   0,   0,   0,   1,   0},
		/*COMPLETE*/ {1,   0,   0,   0,   0,   0},
		/*TEST*/     {1,   0,   0,   0,   0,   0}
		};
		return allowed[current][next];
	}

	void selfTransition(raceState newState) {
		// 1. Reject illegal transitions
		if (!allowedTransition(newState)) {
			return;
		}

		// 2. Check if already in target state
		if (current == newState) {
			return;
		}

		// 3. Set intention to transition
		target = newState;

		// 4. Attempt coordinated change — cast to uint8_t for serialComm API
		txStatus result = txRaceState((uint8_t)target);
		switch (result) {

			case TX_ACKED:
				// Transition confirmed, commit
				entry   = true;     // next loop: run entry logic
				current = target;   // commit new state
				exit    = true;     // run exit logic
				resetTxState(MSG_RACE_STATE);
				return;

			case TX_TIMEOUT:
			case TX_FAILED:
				// Transition failed, revert and abandon
				target = current;
				resetTxState(MSG_RACE_STATE);
				return;

			default:
				// Still TX_SENT or waiting for ACK
				return;
		}
	}

	void rxTransition(raceState newState) {
		if (current == newState) {
			return;
		}
		if (!allowedTransition(newState)) {
			txNack(MSG_RACE_STATE);
			return;
		}
		target  = newState;
		current = newState;
		entry   = true;
		exit    = true;
	}
};

#endif  // STATEMACHINE_H
