# ADR-0004: Heat abort criteria and forceIdle semantics

**Status:** Accepted
**Date:** 2026-05-26

## Context

Two controllers coordinate over UART to run a heat. Either controller can detect a failure that makes the heat result invalid -- a missed serial message, a sensor failure, or a timeout. The system needs a clear, documented rule for *when* to abort a heat versus when to continue with degraded data.

The `forceIdle()` mechanism lets either controller immediately reset to IDLE and fire a best-effort `MSG_RACE_STATE(IDLE)` so the peer can follow. It replaced the earlier "halt and blink" behavior, which required a power cycle to recover.

## Decision

**Abort a heat (call `forceIdle()`) only when the result would be untrustworthy and the heat must be re-run.**

A result is untrustworthy when a required data dependency was permanently lost -- not merely delayed or imprecise.

### Current abort triggers

| Trigger | Controller | Condition | Rationale |
|---------|-----------|-----------|-----------|
| `MSG_RACE_START` TX failure | SC | `TX_TIMEOUT` or `TX_FAILED` | FC sensors never armed. No finish times can be captured. Heat is structurally impossible to complete. |
| `MSG_FOUL` TX failure | SC | `TX_TIMEOUT` or `TX_FAILED` | FC does not know foul state. In REACTION/PRO modes, foul determines the carTime formula; a wrong foul flag produces a wrong result. |
| `MSG_ERROR` received | FC | `rx.lastErrorCode != err_NULL` | SC has already aborted and notified FC. FC follows to stay in sync. |
| Countdown SC-silent timeout | FC | 10 s elapse in COUNTDOWN with no `MSG_RACE_START` | SC appears dead or desynchronized. Sensors were never armed. No meaningful race can proceed. |

### Soft-failure paths (no abort)

| Event | Controller | Rationale |
|-------|-----------|-----------|
| `MSG_LEFT_REACT` / `MSG_RIGHT_REACT` TX failure | SC | Reaction times are record-keeping only. FC's missing-reaction guard fires a best-effort `txError` warning before displaying best-effort times (equal to race times). The heat result, while imprecise, is still valid and displayable. |
| `MSG_DISP_ADVANCE` TX failure | SC | Display advance is operator convenience only; operator can press Start again. |
| `MSG_WINNER` TX failure | FC | Finish times are already on the FC display. SC retries once then shows no win lights; the heat result is preserved on the FC side. |

### MSG_ERROR convention

`MSG_ERROR` is an abort notification, not a general-purpose warning channel. When SC calls `txError()` before `forceIdle()`, it serves as a best-effort "I am aborting" signal to FC. FC that receives `MSG_ERROR` calls `forceIdle()` to stay in sync.

**Asymmetry:** SC does **not** forceIdle on `rx.lastErrorCode`. This is intentional -- FC currently only sends `MSG_ERROR` as the missing-reaction-time warning, which is informational and does not require SC to abort. No current FC code path sends `MSG_ERROR` as a halt signal. If that changes in the future, the convention must be updated here and the SC loop must add a `lastErrorCode` check.

### Known over-conservatism: MSG_FOUL in GATEDROP mode

In `MODE_GATEDROP`, no reaction-time buttons are polled during COUNTDOWN or RACING, so `laneStartUs[]` is never set and `isFoul()` always returns false. The `MSG_FOUL` payload is always `0b00`. A `MSG_FOUL` TX failure in GATEDROP mode aborts a heat whose result would have been correct even without the message -- because FC defaults to no-foul when no foul message is received, matching the always-zero payload.

This over-conservatism is accepted for v1.0. A future refinement could suppress the abort when mode is GATEDROP and foul mask is zero, but the added complexity is not justified for the rare TX-failure scenario.

## Future: Race Manager integration

When the Race Manager (BLE, feature-raceManager branch) is implemented, every `forceIdle()` call should also trigger a BLE notification to the Race Manager so it can mark the heat as incomplete in the bracket. The integration point is inside `stateMachine::forceIdle()` or at the FC's `rx.lastErrorCode` handler.

## Consequences

- Any new abort condition must be evaluated against the "untrustworthy result" criterion before adding a `forceIdle()` call.
- `MSG_ERROR` must not be used as a soft-warning channel if the receiver calls `forceIdle()` on all errors. Either differentiate severity levels or keep `MSG_ERROR` = abort signal.
- The `criticalTxError` variable in both controllers is dead code left over from before this abort design was established. It should be removed (tracked in the project backlog).
