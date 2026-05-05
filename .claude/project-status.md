# Project Status

**Living document.** Claude should update this file whenever work in a session advances, resolves, or changes the status of an item — without being asked. If an item is completed, mark it done and add a brief note. If new issues are discovered, add them.

---

## P0 — Correctness / Must Fix Before First Race

| # | Item | File(s) | Notes |
|---|------|---------|-------|
| P0-1 | `MSG_LEFT_RESULT` / `MSG_RIGHT_RESULT` declared in `serialMsgID` enum but have no RX handler case and are unused | `serialComm.h`, `serialComm.cpp` | Decision needed: remove (keep protocol clean), stub, or implement for Race Manager integration. See `.claude/notes/open_questions.md`. |
| P0-2 | DIALIIN mode (`MODE_DIALIIN`) is defined in `globals.h` but skipped in the mode-button cycle | `startController.cpp` `modeMachine::nextMode()` | Either remove the enum value or complete the implementation before it causes confusion during a race. |

---

## P1 — Important / Pre-Demonstration

| # | Item | File(s) | Notes |
|---|------|---------|-------|
| P1-1 | RFID car identification: `rfid.cpp` is referenced in older docs and CLAUDE.md module table but is **not implemented** — not included in any `#include` | `startController.cpp` | Decide: defer to v2, or stub the module. Remove stale references from docs if deferring. |
| P1-2 | `raceManager/` is an empty/incomplete stub for the future Raspberry Pi BLE race manager | `firmware/raceManager/` | Fine to leave as-is with a clear "future work" marker; just make sure it doesn't confuse the build. |
| P1-3 | RACE_TEST state exists in the state machine on both controllers but immediately returns to IDLE with no test behavior | `startController.cpp`, `finishController.cpp` | Either implement a useful test mode (light/sensor self-test) or document its intended purpose. |

---

## P2 — Polish / Post-Demonstration

| # | Item | File(s) | Notes |
|---|------|---------|-------|
| P2-1 | BLE communication to Race Manager not implemented | `finishController.cpp` | Placeholder comments (`// notifyBLEMode`) mark integration points. Define GATT service before starting. |
| P2-2 | Car ID (`carID`) path in `raceResults` struct is reserved but never populated or transmitted | `finishController.cpp` | Depends on RFID (P1-1) and BLE (P2-1). |
| P2-3 | `calcReactionTimes()` overflow guard may be unnecessary — unsigned subtraction already wraps correctly in C++ | `startController.cpp` | Low risk; review and simplify or add a comment explaining why it's needed. |

---

## Recently Completed

| Date | Item | Summary |
|------|------|---------|
| 2026-05-05 | `finishController.md` doc rewrite | Corrected display design (GPIO, not shift registers), fixed chip name (74HC238 not 74HC137), fixed message count (12 not 14), fixed `rx.*` struct notation, removed stale "missing commas" note |
| 2026-05-05 | `startController.md` doc rewrite | Added TEST state, corrected mode cycle (DIALIIN not reachable), added full pin table, expanded countdown and reaction-time sections |
| 2026-05-05 | `gh` CLI installed | PR creation via `gh pr create` now available |
