#ifndef OUTBOX_H
#define OUTBOX_H

#include "serialComm.h"

// ---------------------------------------------------------------
// Outbox: heat-event message outcome tracking
// ---------------------------------------------------------------
// Both controllers send heat-event messages (race start, fouls, reaction
// times, winner) whose delivery matters differently: some permanent send
// failures make the heat result untrustworthy, some are record-keeping
// only, one deserves a single retry. Those rules -- prose in ADR-0004 --
// are declared here as a per-controller policy table, and the engine turns
// message outcomes into a verdict.
//
// The engine REPORTS; the controller's main loop ACTS (txError + abort).
// It may query and drive the transport (txStatusOf, txResend) but never
// touches race state -- see ADR-0006.
//
// Usage:
//   static const OutboxEntry table[] = { { MSG_FOUL, OutboxPolicy::FATAL,
//                                          err_STATE_TX_TIMEOUT }, ... };
//   static Outbox outbox = { table, entryCount };   // masks start cleared
//
//   The controller's queue wrapper builds the payload, calls the tx*()
//   function, and calls track(id) only if the tx*() returned true (payload
//   construction stays controller-owned). Call checkOutcomes() once per
//   main loop, after txService().

enum class OutboxPolicy : uint8_t {
	FATAL,       // heat result untrustworthy on permanent failure -> verdict reports abort
	TOLERATED,   // record-keeping only -> permanent failure clears silently
	RETRY_ONCE,  // one automatic resend via txResend(), then give up silently
};

// One policy-table row. The table is const controller-owned data; runtime
// tracking state lives in the Outbox bitmasks, not here.
struct OutboxEntry {
	serialMsgID  id;
	OutboxPolicy policy;
	errCode      abortCode;   // reported when a FATAL entry permanently fails
};

struct OutboxVerdict {
	bool    abortRequired;
	errCode code;
};

struct Outbox {
	const OutboxEntry* entries;
	uint8_t            count;         // max 16 (bitmask width)
	uint16_t           pendingMask;   // bit i set = entries[i] awaiting outcome
	uint16_t           retriedMask;   // bit i set = entries[i] already resent once

	// Start tracking a message that was just enqueued. Untracked ids are
	// ignored (not every message a controller sends is a heat event).
	void track(serialMsgID id) {
		for (uint8_t i = 0; i < count; i++) {
			if (entries[i].id == id) {
				pendingMask |=  (uint16_t)(1u << i);
				retriedMask &= ~(uint16_t)(1u << i);
				return;
			}
		}
	}

	bool anyPending() const {
		return pendingMask != 0;
	}

	// Poll the outcome of every tracked message and apply its policy.
	// Returns at most one verdict per call: if two FATAL entries fail in
	// the same pass, the first is reported -- the abort resets the heat
	// anyway, so the second code adds nothing.
	OutboxVerdict checkOutcomes() {
		OutboxVerdict v = { false, err_NULL };
		for (uint8_t i = 0; i < count; i++) {
			uint16_t bit = (uint16_t)(1u << i);
			if (!(pendingMask & bit)) continue;

			txStatus s = txStatusOf(entries[i].id);
			if (s == TX_ACKED) {
				pendingMask &= ~bit;
				continue;
			}
			if (s != TX_TIMEOUT && s != TX_FAILED) continue;   // still unresolved

			switch (entries[i].policy) {
				case OutboxPolicy::FATAL:
					pendingMask &= ~bit;
					if (!v.abortRequired) {
						v.abortRequired = true;
						v.code          = entries[i].abortCode;
					}
					break;
				case OutboxPolicy::TOLERATED:
					pendingMask &= ~bit;
					break;
				case OutboxPolicy::RETRY_ONCE:
					if (!(retriedMask & bit) && txResend(entries[i].id)) {
						retriedMask |= bit;            // resend in flight, keep waiting
					} else {
						pendingMask &= ~bit;           // second failure: give up silently
					}
					break;
			}
		}
		return v;
	}
};

#endif  // OUTBOX_H
