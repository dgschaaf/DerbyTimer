# ADR-0006: State machine declare-intent interface and shared Outbox

**Status:** Accepted (the Outbox portion is a design decision whose
implementation is still pending; the state machine portion is as-built)
**Date:** 2026-07-01

## Context

Coordinated state transitions between the two controllers were correct only
by procedural compliance. Every state handler on both controllers copied the
same three-step ritual -- consume the received state edge, guard, drive the
self-transition -- plus manual hygiene on the `entry`/`exit` one-shot flags
(set by the machine, cleared by hand in every handler). Five handlers per
controller meant ten copies of the ritual.

Two incidents showed the cost of that model:

- A P0-class bug where a handler bypassed the edge-consumption path and fed
  the level-triggered `rx.State` value straight into the follower-transition
  mechanics. The previous heat's stale IDLE re-fired mid-heat and aborted a
  race in progress.
- During the migration itself, deleting the "boilerplate" manual flag clears
  exposed that they were load-bearing: an unconsumed `exit` flag from one
  transition leaked into the next state's handler, which would have disarmed
  the finish-line sensors the moment RACING began.

Separately, each controller carries its own message-outcome bookkeeping
(`PendingTx`): queue-at-event, poll-per-loop, and per-message failure policy
(abort the heat vs. tolerate) hidden inside `checkOutcomes()`. The abort
criteria of ADR-0004 exist only as scattered if-statements.

## Decision

### Declare-intent state machine interface

`stateMachine` (shared by both controllers) exposes exactly four operations
to handlers, plus the abort path:

- `request(raceState next)` -- records intent to transition. Idempotent;
  illegal transitions are ignored (same legality table as before).
- `service(bool holdRx = false)` -- one call per handler pass. In order:
  scrubs any stale `exit` flag, consumes a freshly received state edge
  (follower path), drives a pending request through the send/ACK/commit
  protocol (initiator path). `holdRx` DEFERS the received edge -- never
  discards it -- so the message applies on the first unheld pass.
- `takeEntry()` / `takeExit()` -- return-and-clear accessors, true exactly
  once per transition. Entry persists until the new state's handler consumes
  it; exit is observable only in the pass its transition commits, so
  `takeExit()` must be called after `service()` in the same pass.
- `forceIdle()` -- the abort path, unchanged (see ADR-0004). Aborts are
  deliberately not normal transitions and do not go through `request()`.

The transition mechanics (`selfTransition`, `serviceRx`, `rxTransition`) are
private. The interlock that was previously enforced by every call site doing
three steps in the right order now lives inside the machine, where a call
site cannot get it wrong. The Start Controller's mode machine (`modeSelect`)
mirrors the same shape.

**Rejected: coupling the state machine to the message queue.** The RACING
data-integrity hold ("do not follow COMPLETE while heat messages are still
in flight") could have been automatic if the machine queried the pending
tracker itself. Passing the hold as an argument
(`stm.service(pending.anyPending())`) was chosen instead: the state machine
stays free of messaging dependencies, and the one deliberate hold in the
system is visible in the one handler that needs it.

### Shared Outbox with per-controller policy tables

(Design accepted; implementation pending. Until it lands, the per-controller
`PendingTx` structs described in ADR-0003's consumer pattern remain.)

The two `PendingTx` implementations converge into one engine in
`firmware/lib/shared/`: `track(id)` at the event, `checkOutcomes()` per
loop. Per-message failure policy moves out of code and into a
per-controller policy table (abort-heat vs. tolerate vs. retry-once),
making ADR-0004's abort criteria executable data -- the policy table is the
single place those rows live in code.

**Verdict reporting:** `checkOutcomes()` returns a verdict (e.g. "a fatal
message failed") and the main loop acts on it (calls `forceIdle()`, sends
the error). Rejected alternative: the engine acting on the state machine
directly -- messaging must not own race state.

**`txResend(id)`:** an explicit re-send primitive amends ADR-0003's retry
policy, replacing the re-enqueue idiom for callers that retry after a
terminal status (today only the winner message re-send).

## Consequences

- Any new state handler MUST use the `takeEntry()` / `request()` /
  `service()` / `takeExit()` shape. Direct writes to `target`/`entry`/`exit`
  and per-step transition control are not public API; future architecture
  reviews should not re-suggest exposing them.
- Call-order contract: `takeEntry()` at the top of the handler; `service()`
  once per pass; `takeExit()` after `service()` in the same pass.
- The full coordinated-transition protocol is desktop-testable through the
  public interface against the real serial engine (commit-on-ACK, timeout
  revert, illegal-request rejection, follower commit, holds, stale-edge and
  stale-exit regressions). The interface is the test surface.
- Handlers cannot exercise transition mechanics in isolation anymore; tests
  that need the old surface must drive the machine through `request()` /
  `service()` and the mock wire instead.
- The RACE_TEST self-test's low-level wire ping (raw `txRaceState` /
  `txStatusOf`) is intentionally outside this interface -- it is a test of
  the wire itself, not a state transition.
