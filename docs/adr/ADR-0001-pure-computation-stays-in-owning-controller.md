# ADR-0001: Pure computation stays in its owning controller file

**Status:** Accepted  
**Date:** 2026-05-21

## Context

The finishController firmware separates concerns across files that map to hardware interfaces:
- `sensors.cpp` -- ISR and optical sensor hardware
- `display.cpp` -- BCD decoder and 7-segment display hardware
- `finishController.cpp` -- state machine and race logic

During the `computeHeatResults()` refactor, the question arose whether a pure computation function (no hardware dependencies, no ISR, no Serial) deserves its own file to maximise testability -- it could be compiled and exercised on a PC without any Arduino toolchain.

## Decision

Pure computation stays in `finishController.cpp`, the controller file that owns the domain logic it serves.

File boundaries in this project map to hardware interfaces, not abstraction layers. A new file implies a new hardware concern. A pure function that is only ever called from one controller file does not warrant a new file boundary just because it happens to be hardware-free.

If a future PC-side test harness is built, the extraction can happen at that point with a real consumer to justify the seam.

## Consequences

- `computeHeatResults()` and its associated types (`HeatLaneResult`, `HeatResults`) live in `finishController.cpp` as file-static.
- Future architecture reviews should not re-suggest extracting pure computation into its own file unless a second concrete consumer (e.g., a test harness or a shared library) exists to justify the seam.
- The rule to remember: **one adapter = hypothetical seam; two adapters = real seam.**
