# Race Manager Design

> **Status:** Planning — not yet implemented. `firmware/raceManager/` is a stub placeholder.
> This document captures the intended design and serves as the planning baseline for implementation.
>
> **Branch strategy:** raceManager is its own major feature branch. RFID firmware (a separate major feature branch) is a prerequisite and is assumed to be merged before raceManager work begins.

---

## Overview

The Race Manager is a **full-screen kiosk application** running on a **Raspberry Pi** that serves as the event-level coordinator for a Pinewood Derby race day. It connects wirelessly (BLE) to the Finish Controller, manages the roster, coordinates race modes and heat scheduling, displays race results in real time, and persists results across years.

The two Arduino controllers own all real-time hardware concerns (timing, gate control, sensors, track display). The Race Manager handles everything above that layer: who is racing, in what order, in which mode, what the standings are, and how results are communicated to participants and organizers.

### What the Race Manager does NOT do

- Control gates or start/stop races (that is the Start Controller's job)
- Measure times (that is the Finish Controller's job)
- Drive the per-lane numeric displays at the track (that is the Finish Controller's job)

---

## Repository Paths

| Component | Path |
| --- | --- |
| Race Manager application (RPi) | `software/raceManager/` |
| Finish Controller firmware (BLE additions) | `firmware/finishController/` |
| Stub placeholder (current) | `firmware/raceManager/` — to be removed or repurposed |

---

## Hardware Platform

| Component | Detail |
| --- | --- |
| **Computer** | Raspberry Pi (model TBD — Pi 4 or Pi 5 recommended) |
| **Display** | HDMI monitor; application runs full-screen |
| **Input** | USB keyboard and mouse (wireless dongle acceptable) |
| **Camera** | USB camera — *deferred/backlog*, see [Deferred Features](#deferred-features--backlog) |

The application should auto-launch on boot in kiosk mode, bypassing the graphical desktop. The operator has an in-app option to quit to the Raspberry Pi OS if needed (e.g. for maintenance or updates).

---

## System Context

```
[Start Controller]  ─── UART 115200 ───  [Finish Controller]
 Arduino Nano                              Arduino Nano 33 BLE
 ATmega328P                                nRF52840
                                               │
                                         BLE (GATT)    ← not yet implemented
                                         bidirectional
                                               │
                                       [Race Manager]
                                        Raspberry Pi
                                       (this document)
```

BLE is bidirectional. The Finish Controller notifies the Race Manager of state changes, car IDs, and results. The Race Manager can write race mode and test commands to the Finish Controller when the system is idle.

---

## BLE Interface

The Nano 33 BLE (nRF52840) exposes a GATT service. The Race Manager acts as the BLE central and connects by service UUID. Placeholder comments in `finishController.cpp` (e.g. `// notifyBLEMode(currentMode)`) mark the integration points.

BLE range is expected to be 10–20 feet in a church gym, which is well within BLE 5.0 capability.

### Proposed GATT Service: Derby Race Service

> UUIDs are planning proposals — assign final values at implementation time.

| Characteristic | Direction | Type | When Sent |
| --- | --- | --- | --- |
| **Handshake** | RM ↔ FC (read/write) | `uint8_t` | On connection; RM confirms it is talking to the right device |
| **Race State** | FC → RM (notify) | `uint8_t` | On every state transition (`RACE_IDLE` … `RACE_COMPLETE`) |
| **Race Mode** | FC → RM (notify) | `uint8_t` | When mode changes (`MODE_GATEDROP`, `MODE_REACTION`, `MODE_PRO`, `MODE_DIALIIN`) |
| **Car IDs** | FC → RM (notify) | `uint8_t[2]` | On `RACE_STAGING` entry: `[leftCarID, rightCarID]` from RFID |
| **Heat Result** | FC → RM (notify) | struct | On `RACE_COMPLETE` entry — see payload below |
| **Heartbeat** | FC → RM (notify) | `uint32_t` | Every ~1 s; RM detects connection loss if absent |
| **Malfunction** | FC → RM (notify) | `uint8_t` | On `MSG_ERROR` or `criticalTxError`; carries error code |
| **Set Mode** | RM → FC (write) | `uint8_t` | RM selects race mode — **only accepted when `RACE_IDLE`** |
| **Initiate Test** | RM → FC (write) | `uint8_t` | RM triggers `RACE_TEST` — **only accepted when `RACE_IDLE`** — *deferred* |
| **Test Result** | FC → RM (notify) | TBD | Test outcome data — *deferred* |

### Heat Result Payload

```c
struct BLEHeatResult {
    uint8_t  heatID;                // Assigned by RM, echoed back; 0 if unmanaged
    uint8_t  carIDLeft;             // RFID car ID; 0 if not yet implemented
    uint8_t  carIDRight;
    uint32_t carTimeUsLeft;         // Final car time (reaction-adjusted), µs
    uint32_t carTimeUsRight;
    uint32_t raceTimeUsLeft;        // Raw sensor time, µs
    uint32_t raceTimeUsRight;
    int32_t  reactionTimeUsLeft;    // µs; -1 if not applicable (GATEDROP mode)
    int32_t  reactionTimeUsRight;
    uint8_t  foulMask;              // bit0 = left foul, bit1 = right foul
    uint8_t  winnerMask;            // bit0 = left wins, bit1 = right wins, bit2 = tie
};
```

This mirrors data already computed in `finishController.cpp` `RACE_COMPLETE`.

### BLE Design Constraints

- BLE notify callbacks must not run inline with ISR-driven sensor handling. Queue notifications and send them after `RACE_COMPLETE` entry.
- The ArduinoBLE / mbed BLE stack shares the nRF52840 core with the Arduino sketch. Avoid contention with `display.cpp` GPIO timing (30 µs digit settle).
- Bidirectional writes (Set Mode, Initiate Test) are guarded by state on the FC side — the FC must reject writes received outside `RACE_IDLE`.

---

## Application Platform and Startup

### Technology Choice (Needs Research — OQ-RM1)

The application needs to be a full-screen GUI running natively on the Pi. Framework candidates include Python-native (pygame, tkinter, PyQt), Node.js/Electron, or a web app served to a Chromium kiosk. No decision has been made — research required before Phase 2 begins. Key constraints: full-screen kiosk capability, BLE central role support, bracket/tree rendering, SQLite access, and reasonable performance on a Pi 4/5.

### Kiosk Auto-Launch

- Application launches automatically at boot, before the graphical desktop
- Two approaches: `~/.config/autostart/` entry under a minimal display manager, or a `systemd` service that launches the app directly
- In-app "Quit to Desktop" button terminates the kiosk session and drops to the RPi desktop for maintenance
- In-app "Shutdown" button for clean end-of-event power-off

---

## Power-On / Session Flow

On launch, the application checks for a saved session (in-progress race day state in the local database). It presents:

```
┌─────────────────────────────────┐
│       PINEWOOD DERBY            │
│                                 │
│   [ Continue Race ]  ← only shown if saved session exists
│   [ New Race      ]
│   [ Practice      ]
│   [ Quit to OS    ]
└─────────────────────────────────┘
```

| Option | Description |
| --- | --- |
| **Continue Race** | Restore the last saved session — roster, heats run, bracket state |
| **New Race** | Start fresh; walk through setup wizard (roster, settings, awards) |
| **Practice** | Enter Practice mode immediately — no roster required |
| **Quit to OS** | Exit application to Raspberry Pi desktop |

---

## Racer Management

### Roster Entry

Racers are entered before racing begins. Three entry paths:

1. **Manual entry** — Type name, car number, and optionally age/grade. Optionally capture racer photo and car photo via USB camera (if connected, deferred).
2. **CSV import** — Upload a CSV with columns for name and car number (plus optional age/grade). Import screen shows a preview and flags duplicates.
3. **Previous year's roster** — Load from the persistent database (typically ~80% of names carry over). Operator marks returning vs. new racers, removes departed ones, and adds new entries.

### Racer Record (at entry time)

```
Racer {
    id              : int          // Internal ID, auto-assigned
    name            : string
    carNumber       : int          // May change year to year
    grade           : int?         // Age/grade — optional; changes each year so not a stable identifier
                                   // Consider storing graduating class year and back-calculating grade for display
                                   // Or omit entirely — not essential
    rfidCarID       : bytes?       // Assigned via RFID pairing at check-in; format TBD
    photoPath       : string?      // USB camera capture (deferred)
    carPhotoPath    : string?      // USB camera capture (deferred)
    present         : bool         // Checked in
    year            : int          // Race year
}
```

### RFID Car Pairing

RFID is read by the Start Controller and relayed SC → FC → RM via the BLE Car IDs characteristic (sent at staging). To pair a car to a racer name:

1. Operator opens the **Check-In** screen for a racer
2. Operator places the car on the track (or near the RFID reader on the start controller)
3. SC reads the RFID tag, sends it to FC via serial, FC notifies RM via BLE
4. RM receives the Car ID and associates it with the currently selected racer record
5. Confirmation displayed; racer is marked as checked in

> RFID is a separate major feature branch (firmware P1-1 + P2-2) assumed to be merged before raceManager work begins. RFID car ID format is TBD — it affects the `BLEHeatResult` struct and GATT characteristic sizing.

### Check-In

- Racers check in at the operator's station with their car
- Check-in triggers RFID pairing (above) and optional photo capture
- Absent racers remain in roster but are ineligible for bracket seeding; they can be marked as a Bye

---

## Practice Mode

Available at any time — from the power-on menu or as a toggle during a race day.

- Race mode on the hardware is set to whichever mode the operator selects (RM writes Set Mode → FC)
- Race results are received via BLE and displayed on-screen in real time (times, winner, reaction times)
- **No results are written to the database**
- The kiosk screen shows "PRACTICE — Times not recorded" to avoid confusion
- Returning to a race day session resumes from where it was left off

---

## Time Trial Mode (Gate Drop)

Gate Drop mode on the hardware corresponds to Time Trial mode in the Race Manager. Heats are **rolling and not pre-scheduled** — the operator runs cars through in any order, and the Race Manager matches results to racers by Car ID.

### Time Trial Screen Layout

The main panel shows a roster-style grid with one row per racer:

| Racer | Car # | Run 1 | Run 2 | Run 3 | Run 4 | Run 5 | Best | Last | Trend |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Timmy | 12 | 3.412 | 3.389 | — | — | — | 3.389 | 3.389 | ↓ better |
| Johnny | 7 | 3.501 | — | — | — | — | 3.501 | 3.501 | — |

- **Runs to use** — configurable in settings (default = 5). Column count matches this setting.
- **Best** — fastest time recorded for this racer across all runs
- **Last** — most recent run time
- **Trend** — arrow/color indicating whether last run was better (↓ green) or worse (↑ red) than their previous best

### Car Currently Racing

When the FC sends Car IDs (staging state), the two matching racer rows are **highlighted** on the time trial grid so the operator can see who is on the track.

### Full Runs

Once a racer has completed all configured runs:

- Their row changes color (e.g. gray background, muted text) to indicate they are done
- Their car ID is locked — if FC sends a staging event with that car ID, an **error/warning** is displayed: "Car #12 (Timmy) has completed all runs"
- The operator can choose to override and allow an extra run, or dismiss the error

### Editing Times

At any time the operator can select a racer's row and edit individual run times (e.g. to remove a bad result, correct an entry). All edits are logged.

### Places Calculation

Once all checked-in racers have completed their runs (or after operator manually triggers it):

- Time trial places are calculated (1st, 2nd, 3rd) by best time
- Places are **not displayed automatically** — they are revealed at Race Day Complete
- Calculated places are used to seed the bracket if Bracket mode follows

---

## Bracket Mode (Reaction / Pro Tree)

Reaction mode on the hardware corresponds to Bracket mode in the Race Manager. The bracket is **calculated once at the start of Bracket mode** and does not change except for byes and edits.

### Bracket Generation

1. When the operator enters Bracket mode, the Race Manager generates a single-elimination or double-elimination bracket from all checked-in racers
2. If time trial results are available (fully or partially completed), the bracket is **seeded by time trial best time** — fastest car gets the most favorable bracket position
3. If no time trial data exists, seeding is random or manual

**Elimination format:** Single elimination (default) or double elimination — set in Settings.

### Bracket Screen Layout

Full-screen bracket tree:

- Current matchup **highlighted** (e.g. bright outline)
- On-deck matchup **marked** (e.g. different color)
- Completed matchups show winner name and time
- Remaining matchups show racer names
- Bye slots are labeled "BYE"

### Car ID Verification

When the FC sends Car IDs at staging, the Race Manager checks them against the scheduled matchup:

| Scenario | Action |
| --- | --- |
| Cars match current matchup | Normal — highlight current matchup, proceed |
| Cars match on-deck matchup | Push current matchup to on-deck; new heat becomes current — **allowed** (flexibility for out-of-order) |
| Cars match a valid future matchup (not current or on-deck) | Same as above if the matchup hasn't been run yet |
| Cars match an already-completed matchup | **Error** displayed: "This matchup was already run" |
| Cars don't match any scheduled matchup | **Warning** displayed: "Unrecognized car pairing" — operator can proceed or abort |

### Bye / Forfeit

At any time the operator can select a racer in the bracket and mark them as a **Bye**:

- Their opponent automatically advances
- The Bye racer is noted in results but does not get a time for that matchup
- A racer who goes home early can be given a Bye for all remaining matches

### Foul Rules

| Scenario | Outcome |
| --- | --- |
| One car fouls | Fouling car is **disqualified**; opponent wins automatically |
| Both cars foul | **Re-race** — heat is re-inserted as the current matchup |

### Manual Winner Override

The operator can select any matchup in the bracket and manually set or edit the winner — useful for correcting sensor errors or handling edge cases.

---

## Staging Screen (Matchup Reveal)

When the FC enters `RACE_STAGING` and Car IDs are received, the Race Manager switches to a full-screen **matchup reveal**:

```
┌─────────────────────────────────────────────┐
│                                             │
│   [Car Photo: Timmy's car]                  │
│         TIMMY                               │
│          vs.                                │
│         JOHNNY                              │
│   [Car Photo: Johnny's car]                 │
│                                             │
│        LEFT LANE      RIGHT LANE            │
└─────────────────────────────────────────────┘
```

- Shows racer names in large text
- Shows car photos if available (from check-in)
- Identifies left vs. right lane assignment
- Remains on screen until FC transitions to `RACE_COUNTDOWN` or `RACE_RACING`

---

## Race Day Complete

When the operator selects **Race Day Complete**:

1. The application calculates final results:
   - **Time Trial:** 1st, 2nd, 3rd place by best time
   - **Bracket:** Winner, runner-up (and 3rd/4th if double elimination)
   - **Best Reaction Time:** Fastest reaction time across all bracket heats
2. A **celebration reveal screen** is shown — operator-controlled, screen advances manually to reveal each award (builds suspense)
3. Optionally, the operator enters the **Best in Show** award winner(s) and places (judged separately before closing)
4. All results are saved to the persistent database (see [Persistent Database](#persistent-database))

### Best in Show

- Separate judged award; not based on race times
- Operator enters winner name and 2nd/3rd place if applicable before closing the session
- Saved to database alongside race results

---

## UI State Machine

The Race Manager enables and disables UI elements based on the current `raceState` received from the FC. The RM tracks the last-known state from BLE notifications.

| State | RM UI Behavior |
| --- | --- |
| `RACE_IDLE` | "Set Mode" button active; roster and settings editable; heats can be manually adjusted |
| `RACE_STAGING` | Show Staging/Matchup Reveal screen; highlight current racers in time trial grid; no editing |
| `RACE_COUNTDOWN` | Show countdown indicator; disable all editing; lock current matchup display |
| `RACE_RACING` | Show "Racing…" indicator; disable all editing |
| `RACE_COMPLETE` | Receive Heat Result via BLE; update time trial grid or bracket; enable error-correction actions (Discard, Re-run, Skip) |
| BLE disconnected | Show connection warning banner; disable all BLE-dependent actions; time trial grid goes read-only |

---

## Deferred Features / Backlog

The following are intentionally out of scope for the initial raceManager release. They are documented here to capture intent and avoid re-discussing them from scratch.

### Double Elimination

Support a losers-bracket in addition to the winners bracket. Adds significant bracket-state complexity. Default will remain single elimination.

### Lane Bias Analysis

Post-event analytics to detect whether one lane has an inherent speed advantage.

**Methodology:**

- **Track-level bias:** Average all left-lane times across all heats; average all right-lane times across all heats; report the difference.
- **Per-racer bias:** For each racer who ran both lanes, average their left-lane times and average their right-lane times; report the per-racer delta. This separates car performance from lane advantage.

**Correction options (also deferred, decision TBD):**

- Equal runs per car — scheduling constraint ensuring each car runs each lane the same number of times (no time modification)
- Correction factor — apply a lane-bias offset to raw times before calculating standings (more powerful, needs a defensible methodology)

### Dial-In Mode

Gate Drop timing used to calculate a speed handicap per racer. Handicap would be applied to un-bias time trial standings or bracket seeding. Requires firmware changes (`MODE_DIALIIN` is defined in `raceTypes.h` but currently skipped in the mode cycle — P0-2).

### USB Camera

A USB camera connected to the RPi (not practical on the Arduino) would enable:

- **Check-in photos:** Racer photo and car photo captured at check-in
- **Slow-motion replay:** Ring-buffer video capture during racing; replay last 1–2 seconds in slow motion after `RACE_COMPLETE`

### Test Mode (from Race Manager)

RM-initiated hardware test: send Initiate Test to FC when `RACE_IDLE`, display test results on-screen. Details TBD pending `RACE_TEST` firmware implementation (P1-3).

---

## Error Handling

Operator-side corrections available after a heat completes (during `RACE_COMPLETE` state on FC):

| Action | Description |
| --- | --- |
| **Discard / Erase time** | Mark one or both lane results as void; exclude from standings; car's run slot is freed for re-use |
| **Re-run race** | Flag the heat for re-run; re-insert as the next scheduled heat |
| **Skip heat** | Remove the heat from the schedule without re-running (e.g. car scratched mid-event) |
| **Edit time** | Manually override a specific run time in the time trial grid |
| **Set winner** | Manually set bracket matchup winner (overrides sensor result) |

These are Race Manager UI actions only — no signals are sent to the Arduino controllers.

---

## Export

At any time after heats are run (and required after Race Day Complete):

- **CSV export** — all heat results, per-racer times, standings, reaction times
- **PDF export** (stretch) — formatted results sheet suitable for printing and posting
- Export includes: racer name, car number, all run times, best time, time trial place, bracket result, best reaction time, foul count

---

## Persistent Database

Results are saved locally on the RPi (SQLite is a natural fit). The database persists across sessions and years.

### Racer Record (persistent)

```
Racer {
    id              : int
    name            : string
    carNumber       : int          // May change year to year; stored per season result
    grade           : int?         // Optional; see note in Racer Record above
    photoPath       : string?      // Deferred
    carPhotoPath    : string?      // Deferred
}
```

### Season Record (per year)

```
SeasonResult {
    racerID         : int          // FK → Racer
    year            : int
    bestTimeTrial   : µs?          // Best time trial time
    timeTrialPlace  : int?         // 1st, 2nd, 3rd, etc.
    bracketPlace    : int?         // 1 = winner, 2 = runner-up, etc.
    bestReactTime   : µs?          // Best reaction time across bracket heats
    bestInShowPlace : int?         // If awarded
}
```

### Heat Record

```
HeatResult {
    id              : int
    year            : int
    mode            : enum (TimeTrial, Bracket, Practice)
    heatSeq         : int          // Heat number within the event
    leftRacerID     : int?         // FK → Racer; null if car ID not matched
    rightRacerID    : int?
    leftCarTime     : µs
    rightCarTime    : µs
    leftReactTime   : µs?          // null in GATEDROP mode
    rightReactTime  : µs?
    leftFoul        : bool
    rightFoul       : bool
    winner          : enum (Left, Right, Tie, Void, Rerun)
    notes           : string?      // Operator edit notes
}
```

---

## Settings

Configurable per event (stored with the session, not globally):

| Setting                 | Default | Description                                   |
| ----------------------- | ------- | --------------------------------------------- |
| Time trial runs per car | 5       | How many Gate Drop runs are recorded per car  |

---

## Implementation Phases

### Phase 1 — BLE Foundation (Firmware, prerequisite)

- [ ] Define GATT service UUID and characteristic UUIDs in `finishController.cpp`
- [ ] Implement `notifyBLEState()`, `notifyBLECarIDs()`, `notifyBLEResult()`, heartbeat at placeholder sites
- [ ] Implement writable characteristics: Set Mode and Initiate Test; guard with `RACE_IDLE` check
- [ ] BLE connection event handler; `bleConnected` flag
- [ ] Validate BLE notify timing does not interfere with display ISR (30 µs settle)
- [ ] Reference: P2-1 in project-status.md

### Phase 2 — Race Manager Skeleton (RPi)

- [ ] Decide application framework (see OQ-RM1)
- [ ] Implement BLE central via Python `bleak`; connect by service UUID; subscribe to all notifies
- [ ] Kiosk auto-launch at boot (systemd service or autostart)
- [ ] Power-on menu: Continue / New Race / Practice / Test / Quit to OS
- [ ] Log raw BLE events to console — validate full data path end-to-end

### Phase 3 — Roster and Time Trial

- [ ] Racer entry: manual, CSV import, previous year import from database
- [ ] RFID pairing workflow at check-in (prerequisite: RFID firmware branch merged)
- [ ] Settings UI (runs per car)
- [ ] Time trial grid: runs, best, last, trend, color for completed rows
- [ ] Car ID → racer row highlighting during staging
- [ ] Full-runs error/warning when car ID is already complete
- [ ] Time trial places calculation (hidden until reveal)

### Phase 4 — Bracket and Staging Screen

- [ ] Bracket generation (single elimination, seeded from time trial if available)
- [ ] Bracket screen: tree display, current/on-deck markings
- [ ] Car ID verification against scheduled matchup; out-of-order flexibility
- [ ] Staging / matchup reveal screen (full-screen with names and car photos)
- [ ] Bye / forfeit handling
- [ ] Foul logic (DQ, re-race on double foul)
- [ ] Manual winner override

### Phase 5 — Race Day Complete and Database

- [ ] Race Day Complete flow: standings calculation, celebration reveal screen
- [ ] Best in Show entry
- [ ] Persistent SQLite database: season records, heat history, multi-year racer records
- [ ] Previous year roster import from database
- [ ] Export to CSV
- [ ] Practice mode toggle from within a race day session
- [ ] PDF export (stretch)

---

## Open Questions

| # | Question | Why it matters |
| --- | --- | --- |
| OQ-RM1 | What application framework? Needs research — no decision made. | Drives all of Phase 2 architecture; must be resolved before coding begins |
| OQ-RM2 | RFID car ID format — single byte, multi-byte UID, other? | Affects `BLEHeatResult` struct and GATT characteristic sizing; resolved when RFID branch is designed |
| OQ-RM3 | Grade/age field: store graduating class and back-calculate, store raw grade, or omit entirely? | Low urgency; doesn't affect race logic |

---

## Dependencies and Blockers

| Item | Status | Blocks |
| --- | --- | --- |
| BLE GATT service defined on FC | Not started | All of Phase 1 and everything after |
| Application framework decision (OQ-RM1) | Needs research | All of Phase 2 |
| RFID firmware branch (P1-1, P2-2) | Not started — own branch | RFID pairing at check-in; Car ID verification in bracket; assumed merged before raceManager branch |
| Reaction time sentinel fixed (P1-4) | Not started | Correct reaction time in BLE payload |
