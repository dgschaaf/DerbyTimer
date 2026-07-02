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

	// ---- Declare-intent interface ----
	// Handlers declare where the machine should go (request) and give it
	// one chance per pass to get there (service). The coordinated-transition
	// protocol -- consume the peer's message, then drive a pending intent
	// through send/ACK/commit -- lives inside the machine, so a call site
	// cannot get the ordering wrong.

	// Record intent to transition. Idempotent -- safe to call every pass.
	// Illegal transitions are ignored (same legality table selfTransition
	// enforces).
	void request(raceState next) {
		if (next == current) return;
		if (!allowedTransition(next)) return;
		target = next;
	}

	// One call per handler pass: (1) scrub any stale exit flag -- every
	// commit raises exit for the state being LEFT, and a state that has no
	// exit actions never consumes it, so without the scrub the NEXT state
	// would inherit it and fire its own exit actions on arrival; (2) unless
	// holdRx, consume a freshly received state edge (follower path);
	// (3) drive any pending request through the coordinated transition
	// protocol (initiator path). holdRx DEFERS the received edge -- never
	// discards it -- so the message applies on the first unheld pass. Used
	// for the RACING data-integrity hold: lane results must finish flowing
	// before a state change is honored.
	void service(bool holdRx = false) {
		exit = false;
		if (!holdRx) serviceRx();
		if (target != current) selfTransition(target);
	}

	// Return-and-clear accessors for the entry/exit flags: true exactly
	// once per transition. Clearing is part of the read, so entry/exit
	// work cannot run twice and the flag reset cannot be forgotten.
	// Lifetimes differ: entry persists until the new state's handler
	// consumes it (usually the next pass); exit is only observable in the
	// pass its transition commits -- call takeExit() AFTER service() in
	// the same pass.
	bool takeEntry() {
		if (!entry) return false;
		entry = false;
		return true;
	}

	bool takeExit() {
		if (!exit) return false;
		exit = false;
		return true;
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
