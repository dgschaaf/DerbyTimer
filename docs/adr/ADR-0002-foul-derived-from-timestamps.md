# ADR-0002: Foul status is derived from timestamps, not stored as a flag

**Status:** Accepted  
**Date:** 2026-05-21

## Context

During a heat, a foul occurs when a driver triggers their gate before the GO signal fires. The
start controller must track both whether a foul happened and how early the driver jumped (the
reaction time magnitude).

The original design stored this as a separate boolean (`leftFoul`, `rightFoul`) in `raceResultsData`,
set atomically alongside the gate-drop timestamp in `handleEarlyStarts()`. The invariant — "if
`leftFoul` is true, then `leftStartUs < raceStartUs`" — was enforced only by calling three
functions in the right order across two states (COUNTDOWN and RACING). Any drift between the
stored flag and the actual timestamp relationship would silently corrupt reaction time calculations.

## Decision

Foul status is derived from the timestamp relationship, not stored. `raceTimingData.isFoul(lane)`
computes from the two timestamps at call time:

- `laneStartUs[lane] == 0`: not triggered — return false  
- `raceStartUs == 0`: GO has not fired yet, but lane was triggered — always a foul  
- otherwise: `(int32_t)(laneStartUs[lane] - raceStartUs) < 0` — triggered before GO

`reactionTimeUs(lane)` subtracts in the correct direction based on `isFoul()`. Both are methods
on `raceTimingData`, which owns all three timestamps. `raceResultsData` and `calcReactionTimes()`
are eliminated.

## Consequences

- The invariant "foul flag matches timestamp direction" is now structural — you cannot have a
  stale foul flag because there is no stored flag.
- `laneStartUs[lane] == 0` is the sentinel for "not triggered." Any real `micros()` value at
  race time is nonzero. A zero received by the finish controller signals a data error (re-race).
- `isFoul()` uses signed subtraction (`(int32_t)(laneStartUs[lane] - raceStartUs) < 0`) rather
  than direct comparison (`<`). Direct comparison breaks across a `micros()` uint32_t wrap-around
  (~71 min). The stored-boolean design avoided this because the flag was captured at event time;
  the derived design defers determination to call time, so the comparison must be wrap-safe.
  `reactionTimeUs()` is already safe — unsigned subtraction wraps correctly for elapsed time.
- Future architecture reviews should not re-introduce a stored foul boolean. If the derivation
  ever needs to change, change `isFoul()` — one place, enforced everywhere.
