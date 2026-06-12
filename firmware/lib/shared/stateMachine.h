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
		// →IDLE column: STAGING→IDLE is the normal operator-abort path;
		// COUNTDOWN→IDLE and RACING→IDLE are emergency abort paths.
		static constexpr bool allowed[6][6] = {
		/* FROM\TO:  IDLE STAG CNTD RACE CMPL TEST */
		/*IDLE*/     {0,   1,   0,   0,   0,   1},
		/*STAGING*/  {1,   0,   1,   0,   0,   0},
		/*COUNTDOWN*/{1,   0,   0,   1,   0,   0},
		/*RACING*/   {1,   0,   0,   0,   1,   0},
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

		// 4. Enqueue coordinated change (no-op if already in flight)
		txRaceState((uint8_t)target);

		// 5. Check current TX status
		switch (txStatusOf(MSG_RACE_STATE)) {

			case TX_ACKED:
				// Transition confirmed, commit
				entry   = true;
				current = target;
				exit    = true;
				return;

			case TX_TIMEOUT:
			case TX_FAILED:
				// Transition failed, revert and abandon
				target = current;
				return;

			default:
				// TX_NONE, TX_SENT, TX_NACKED -- still waiting
				return;
		}
	}

	// Consume a freshly received MSG_RACE_STATE, if any. rx.State is
	// level-triggered (it holds the last received value forever), so a stale
	// value fed straight to rxTransition() re-fires old transitions -- e.g.
	// the previous heat's IDLE aborting the current heat from RACING. The
	// StateChanged flag makes the message edge-triggered: rxSerial() sets it,
	// this method consumes it exactly once. Call this instead of passing
	// rx.State to rxTransition() directly.
	void serviceRx() {
		if (!rx.StateChanged) return;
		rx.StateChanged = false;
		rxTransition((raceState)rx.State);
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

	void forceIdle() {
		// Abort path: resets local state to IDLE immediately without waiting
		// for an ACK. Fires a best-effort MSG_RACE_STATE(IDLE) so the other
		// controller can follow; if that message is lost, the other controller
		// recovers via its own timeout paths.
		if (current == RACE_IDLE) return;
		current = RACE_IDLE;
		target  = RACE_IDLE;
		entry   = true;
		exit    = false;
		txRaceState((uint8_t)RACE_IDLE);
	}
};

#endif  // STATEMACHINE_H
