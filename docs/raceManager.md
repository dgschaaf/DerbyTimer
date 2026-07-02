# Race Manager Design

> **Status:** Planning -- not yet implemented. `firmware/raceManager/` is a stub placeholder.
> This document captures the intended design and serves as the planning baseline for implementation.
>
> **Branch strategy:** raceManager is its own major feature branch (`feature-raceManager`). RFID firmware (a separate major feature branch) is NOT a prerequisite: the Race Manager treats manual car matching (operator selects which cars are on the track) as a first-class workflow, and RFID-based Car ID matching layers on top of it later. The manual path doubles as the degraded mode if RFID fails on race night.

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
| Stub placeholder (current) | `firmware/raceManager/` -- to be removed or repurposed |

---

## Hardware Platform

| Component | Detail |
| --- | --- |
| **Computer** | Raspberry Pi (model TBD -- Pi 4 or Pi 5 recommended) |
| **Display** | HDMI monitor; application runs full-screen |
| **Input** | USB keyboard and mouse (wireless dongle acceptable) |
| **Camera** | USB camera -- *deferred/backlog*, see [Deferred Features](#deferred-features--backlog) |

The application should auto-launch on boot in kiosk mode, bypassing the graphical desktop. The operator has an in-app option to quit to the Raspberry Pi OS if needed (e.g. for maintenance or updates).

---

## System Context

```
[Start Controller]  ─── UART 115200 ───  [Finish Controller]
 Arduino Nano                              Arduino Nano 33 BLE
 ATmega328P                                nRF52840
                                               │
                                         BLE (GATT)    <- not yet implemented
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

BLE range is expected to be 10-20 feet in a church gym, which is well within BLE 5.0 capability.

### Proposed GATT Service: Derby Race Service

> UUIDs are planning proposals -- assign final values at implementation time.

| Characteristic | Direction | Type | When Sent |
| --- | --- | --- | --- |
| **Handshake** | RM <-> FC (read/write) | `uint8_t` | On connection; RM confirms it is talking to the right device |
| **Race State** | FC -> RM (notify) | `uint8_t` | On every state transition (`RACE_IDLE` ... `RACE_COMPLETE`) |
| **Race Mode** | FC -> RM (notify) | `uint8_t` | When mode changes (`MODE_GATEDROP`, `MODE_REACTION`, `MODE_PRO`, `MODE_DIALIIN`) |
| **Car IDs** | FC -> RM (notify) | `uint8_t[2]` | On `RACE_STAGING` entry: `[leftCarID, rightCarID]` from RFID |
| **Heat Result** | FC -> RM (notify) | struct | On `RACE_COMPLETE` entry -- see payload below |
| **Heartbeat** | FC -> RM (notify) | `uint32_t` | Every ~1 s; RM detects connection loss if absent |
| **Malfunction** | FC -> RM (notify) | `uint8_t` | On `MSG_ERROR` or `criticalTxError`; carries error code |
| **Set Mode** | RM -> FC (write) | `uint8_t` | RM selects race mode -- **only accepted when `RACE_IDLE`** |
| **Initiate Test** | RM -> FC (write) | `uint8_t` | RM triggers `RACE_TEST` -- **only accepted when `RACE_IDLE`** -- *deferred* |
| **Test Result** | FC -> RM (notify) | TBD | Test outcome data -- *deferred* |

### Heat Result Payload

```c
struct BLEHeatResult {
    uint8_t  heatID;                // Assigned by RM, echoed back; 0 if unmanaged
    uint8_t  carIDLeft;             // RFID car ID; 0 if not yet implemented
    uint8_t  carIDRight;
    uint32_t carTimeUsLeft;         // Final car time (reaction-adjusted), us
    uint32_t carTimeUsRight;
    uint32_t raceTimeUsLeft;        // Raw sensor time, us
    uint32_t raceTimeUsRight;
    uint32_t reactionTimeUsLeft;    // us; meaningful only if flagged in reactionValidMask
    uint32_t reactionTimeUsRight;
    uint8_t  reactionValidMask;     // bit0 = left reaction meaningful, bit1 = right (OQ-RM4, resolved)
    uint8_t  foulMask;              // bit0 = left foul, bit1 = right foul
    uint8_t  winnerMask;            // bit0 = left wins, bit1 = right wins, bit2 = tie, bit3 = no result (double foul / aborted)
};

// Note: foul reaction time (how early the driver jumped) is included in reactionTimeUsLeft/Right
// even for foul lanes. The track display blanks foul lanes (BCD hardware cannot label the number),
// but the Race Manager SHOULD surface foul reaction time with a proper label ("jumped X.XXX s early")
// as coaching feedback. Use foulMask to determine which lanes to label accordingly.
```

> **OQ-RM4 (resolved)** -- Reaction-time validity is signaled by an explicit `reactionValidMask` byte (bit0 = left meaningful, bit1 = right). Inference from `foulMask` + mode was rejected because a reaction message lost on the UART link reads as 0 -- indistinguishable from a genuine 0.000 s reaction, which would put a false time in the database. A foul lane's reaction time IS valid (it is the early-jump coaching value). No such mask exists in firmware yet; creating it is part of the firmware BLE milestone.

This mirrors data already computed in `finishController.cpp` `RACE_COMPLETE`.

### BLE Design Constraints

- BLE notify callbacks must not run inline with ISR-driven sensor handling. Queue notifications and send them after `RACE_COMPLETE` entry.
- The ArduinoBLE / mbed BLE stack shares the nRF52840 core with the Arduino sketch. Avoid contention with `display.cpp` GPIO timing (30 us digit settle).
- Bidirectional writes (Set Mode, Initiate Test) are guarded by state on the FC side -- the FC must reject writes received outside `RACE_IDLE`.

---

## Application Platform and Startup

### Technology Choice (Resolved -- OQ-RM1)

**Python backend + web UI in Chromium kiosk mode.** The backend (Python) owns BLE central (`bleak`), SQLite persistence, and race logic; the frontend is HTML/CSS/JS served locally and displayed full-screen by Chromium `--kiosk` on the Pi. Rationale: bracket trees, roster grids, and celebration reveal screens are far easier to build well in HTML/CSS than in desktop GUI toolkits, and the whole application can be developed and tested on a Windows PC in a normal browser -- no Pi or track hardware required until deployment. Accepted tradeoff: two-language stack (Python + JS). The Pi is reachable over Ethernet/SSH on the home network, so later-stage development can run on the Pi directly.

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
│   [ Continue Race ]  <- only shown if saved session exists
│   [ New Race      ]
│   [ Practice      ]
│   [ Quit to OS    ]
└─────────────────────────────────┘
```

| Option | Description |
| --- | --- |
| **Continue Race** | Restore the last saved session -- roster, heats run, bracket state |
| **New Race** | Start fresh; walk through setup wizard (roster, settings, awards) |
| **Practice** | Enter Practice mode immediately -- no roster required |
| **Quit to OS** | Exit application to Raspberry Pi desktop |

---

## Racer Management

### Roster Entry

Racers are entered before racing begins. Three entry paths:

1. **Manual entry** -- Type name, car number, and optionally age/grade. Optionally capture racer photo and car photo via USB camera (if connected, deferred).
2. **CSV import** -- Upload a CSV with columns for name and car number (plus optional age/grade). Import screen shows a preview and flags duplicates.
3. **Previous year's roster** -- Load from the persistent database (typically ~80% of names carry over). Operator marks returning vs. new racers, removes departed ones, and adds new entries.

### Records Created at Entry Time

Roster entry creates (or, for returning racers, reuses) a persistent **Racer** record and always creates a fresh annual **Car** record for this year -- name and photo live on the Racer; carNumber, grade, rfidCarID, car photo, and check-in state live on the Car. Field-level definitions are in [Persistent Database](#persistent-database).

### RFID Car Pairing

RFID is read by the Start Controller and relayed SC -> FC -> RM via the BLE Car IDs characteristic (sent at staging). To pair a car to a racer name:

1. Operator opens the **Check-In** screen for a racer
2. Operator places the car on the track (or near the RFID reader on the start controller)
3. SC reads the RFID tag, sends it to FC via serial, FC notifies RM via BLE
4. RM receives the Car ID and associates it with the currently selected racer record
5. Confirmation displayed; racer is marked as checked in

> RFID is a separate major feature branch assumed to be merged before raceManager work begins. RFID car ID format is TBD -- it affects the `BLEHeatResult` struct and GATT characteristic sizing.

### Check-In

- Racers check in at the operator's station with their car
- Check-in triggers RFID pairing (above) and optional photo capture
- Absent racers remain in roster but are ineligible for bracket seeding; they can be marked as a Bye

---

## Practice Mode

Available at any time -- from the power-on menu or as a toggle during a race day.

- Race mode on the hardware is set to whichever mode the operator selects (RM writes Set Mode -> FC)
- Race results are received via BLE and displayed on-screen in real time (times, winner, reaction times)
- **No results are written to the database**
- The kiosk screen shows "PRACTICE -- Times not recorded" to avoid confusion
- Returning to a race day session resumes from where it was left off

---

## Time Trial Mode (Gate Drop)

Gate Drop mode on the hardware corresponds to Time Trial mode in the Race Manager. Heats are **rolling and not pre-scheduled** -- the operator runs cars through in any order, and the Race Manager matches results to racers by Car ID.

### Time Trial Screen Layout

The main panel shows a roster-style grid with one row per racer:

| Racer | Car # | Run 1 | Run 2 | Run 3 | Run 4 | Run 5 | Best | Last | Trend |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Timmy | 12 | 3.412 | 3.389 | -- | -- | -- | 3.389 | 3.389 | down better |
| Johnny | 7 | 3.501 | -- | -- | -- | -- | 3.501 | 3.501 | -- |

- **Runs to use** -- configurable in settings (default = 5). Column count matches this setting.
- **Best** -- fastest time recorded for this racer across all runs
- **Last** -- most recent run time
- **Trend** -- arrow/color indicating whether last run was better (down green) or worse (up red) than their previous best

### Car Currently Racing

When the FC sends Car IDs (staging state), the two matching racer rows are **highlighted** on the time trial grid so the operator can see who is on the track.

### Manual Car Matching (primary until RFID lands; fallback forever)

The operator **pre-selects** the next pairing on the roster grid before the heat runs ("next up: #12 left lane, #7 right lane"). The incoming heat result auto-attaches to the pre-selected cars; mistakes are corrected with the normal edit tools. This mirrors the future RFID staging flow exactly -- RFID merely fills the pairing in automatically -- so the UI and attribution data path do not change when RFID lands. In Bracket mode the current matchup already is the pre-selection. If RFID fails on race night, manual pre-select is the degraded mode.

### Solo Runs

A time-trial heat may run with a single car (odd roster count, make-up runs). The operator pre-selects one car plus an empty lane; the empty lane times out on the track and its maxRaceTimeUs DNF result is **auto-discarded** -- no run is recorded for the empty lane. Bracket heats always have two cars (a missing opponent is a Bye, not a solo heat). Bench verification that an empty lane stages and completes normally in GATEDROP mode belongs to the hardware-integration milestone.

### Full Runs

Once a racer has completed all configured runs:

- Their row changes color (e.g. gray background, muted text) to indicate they are done
- Their car ID is locked -- if FC sends a staging event with that car ID, an **error/warning** is displayed: "Car #12 (Timmy) has completed all runs"
- The operator can choose to override and allow an extra run, or dismiss the error

### Editing Times

At any time the operator can select a racer's row and edit individual run times (e.g. to remove a bad result, correct an entry). All edits are logged.

### Incomplete Runs

Racers who have not completed all configured runs are **still eligible for bracket seeding** -- they are seeded by their best available time. Missing runs are left as null in the database; no placeholder entry is required.

Before the operator transitions from Time Trial to Bracket mode, the Race Manager warns if any racers have fewer than the configured number of runs. This is a warning only -- the operator can proceed. The operator may also mark any racer as **Scratch** at this point (or at any time), which removes them from the bracket entirely.

### Places Calculation

Once all checked-in, non-scratched racers have completed their runs (or after operator manually triggers it):

- Time trial places are calculated (1st, 2nd, 3rd) by best available time
- Places are **not displayed automatically** -- they are revealed at Race Day Complete
- Calculated places are used to seed the bracket if Bracket mode follows

---

## Bracket Mode (Reaction / Pro Tree)

Reaction mode on the hardware corresponds to Bracket mode in the Race Manager. The bracket is **calculated once at the start of Bracket mode** and does not change except for byes and edits.

### Bracket Generation

1. When the operator enters Bracket mode, the Race Manager generates a single-elimination or double-elimination bracket from all checked-in, non-scratched cars
2. Bracket size is the next power of 2 at or above the car count (e.g. 18 cars -> 32-car bracket)
3. Byes fill the remaining slots and are awarded to the **top seeds** (fastest time trial times) -- the fastest cars are rewarded with a first-round bye
4. If time trial results are available (fully or partially completed), seeding is by best available time; if no time trial data exists, seeding is random or manual

**Elimination format:** Single elimination (default) or double elimination -- set in Settings.

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
| Cars match current matchup | Normal -- highlight current matchup, proceed |
| Cars match on-deck matchup | Push current matchup to on-deck; new heat becomes current -- **allowed** (flexibility for out-of-order) |
| Cars match a valid future matchup (not current or on-deck) | Same as above if the matchup hasn't been run yet |
| Cars match an already-completed matchup | **Error** displayed: "This matchup was already run" |
| Cars don't match any scheduled matchup | **Warning** displayed: "Unrecognized car pairing" -- operator can proceed or abort |

### Bye / Forfeit

At any time the operator can select a racer in the bracket and mark them as a **Bye**:

- Their opponent automatically advances
- The Bye racer is noted in results but does not get a time for that matchup
- A racer who goes home early can be given a Bye for all remaining matches

### Foul Rules

| Scenario | Outcome |
| --- | --- |
| One car fouls | Fouling car is **disqualified** (DQ); opponent wins automatically; result recorded |
| Both cars foul | **Rerun** -- no result recorded; matchup returned immediately to the front of the queue as "now racing" (the on-deck matchup is not advanced); repeated until a valid result is produced |

### Manual Winner Override

The operator can select any matchup in the bracket and manually set or edit the winner -- useful for correcting sensor errors or handling edge cases.

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
   - **Best Reaction Time:** Fastest single reaction time of the night (event-wide minimum across all racers' personal best reaction times). Consolation award -- displayed alongside each racer's best and average reaction time so kids can compare.
2. A **celebration reveal screen** is shown -- operator-controlled, screen advances manually to reveal each award (builds suspense)
3. Optionally, the operator enters the **Best in Show** award winner(s) and places (judged separately before closing)
4. All results are saved to the persistent database (see [Persistent Database](#persistent-database))

### Best in Show

- Separate judged award for car design, creativity, craftsmanship, humor, or other subjective qualities -- entirely independent of race results
- Judged by an external panel designated by the organizer (not the Race Manager operator)
- Configurable divisions per event: zero, one, or two age-based divisions (typical split: Younger 3rd-5th grade, Older 6th-8th grade). All cars race together regardless of division.
- Up to 3 places per division; 2nd and 3rd are optional -- operator leaves blank if not awarded
- Operator enters winner names manually after judging, before closing the session
- Saved to database alongside race results

---

## UI State Machine

The Race Manager enables and disables UI elements based on the current `raceState` received from the FC. The RM tracks the last-known state from BLE notifications.

| State | RM UI Behavior |
| --- | --- |
| `RACE_IDLE` | "Set Mode" button active; roster and settings editable; heats can be manually adjusted |
| `RACE_STAGING` | Show Staging/Matchup Reveal screen; highlight current racers in time trial grid; no editing |
| `RACE_COUNTDOWN` | Show countdown indicator; disable all editing; lock current matchup display |
| `RACE_RACING` | Show "Racing..." indicator; disable all editing |
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

- Equal runs per car -- scheduling constraint ensuring each car runs each lane the same number of times (no time modification)
- Correction factor -- apply a lane-bias offset to raw times before calculating standings (more powerful, needs a defensible methodology)

### Dial-In Mode

Gate Drop timing used to calculate a speed handicap per racer. `MODE_DIALIIN` is defined in `raceTypes.h` and is fully supported by both controllers -- it is intentionally not reachable via the mode button because the handicap calculation requires historical run data that only the Race Manager holds. The RM activates it by writing the Set Mode BLE characteristic when the system is idle.

> **Mechanic TBD -- research required.** Two candidate approaches: (a) adjust race results mathematically by each car's handicap after the heat; (b) adjust the countdown light timing per lane so slower cars get an earlier GO signal (requires hardware support for per-lane countdown delay). Real drag strip "dial-in" conventions should be reviewed before committing to either approach. This feature was suggested by a participant and is intentionally vague until researched.

### USB Camera

A USB camera connected to the RPi (not practical on the Arduino) would enable:

- **Check-in photos:** Racer photo and car photo captured at check-in
- **Slow-motion replay:** Ring-buffer video capture during racing; replay last 1-2 seconds in slow motion after `RACE_COMPLETE`

### Test Mode (from Race Manager)

RM-initiated hardware test: send Initiate Test to FC when `RACE_IDLE`, display test results on-screen. The `RACE_TEST` self-test itself is implemented in firmware; what remains TBD is the BLE Initiate Test / Test Result characteristic design.

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

These are Race Manager UI actions only -- no signals are sent to the Arduino controllers.

---

## Export

At any time after heats are run (and required after Race Day Complete):

- **CSV export** -- all heat results, per-racer times, standings, reaction times
- **PDF export** (stretch) -- formatted results sheet suitable for printing and posting
- Export includes: racer name, car number, all run times, best time, time trial place, bracket result, best reaction time, average reaction time, foul count

---

## Persistent Database

Results are saved locally on the RPi (SQLite is a natural fit). The database persists across sessions and years.

The schema follows the glossary split (OQ-RM5, resolved): the **Racer** is the persistent person, the **Car** is the annual racing entity. Heat results reference Cars, not Racers; lifetime records roll up through Car -> Racer.

### Racer Record (persistent, one per person across years)

```text
Racer {
    id              : int
    name            : string
    photoPath       : string?      // Deferred
}
```

### Car Record (annual, one per racer per session/year)

```text
Car {
    id              : int
    racerID         : int          // FK -> Racer
    year            : int
    carNumber       : int          // Painted-on number; may change year to year
    grade           : int?         // Optional; OQ-RM3 resolved: grade is annual, so it
                                   // lives here; returning-roster import prefills
                                   // last year's grade + 1 for confirmation
    rfidCarID       : bytes?       // Session-scoped Car ID from the random RFID sticker
                                   // assigned at check-in; survives session resume;
                                   // NOT a durable cross-year identifier
    carPhotoPath    : string?      // Deferred
    present         : bool         // Checked in
}
```

### Season Record (per car = per racer per year)

```text
SeasonResult {
    carID           : int          // FK -> Car
    bestTimeTrial   : us?          // Best time trial time
    timeTrialPlace  : int?         // 1st, 2nd, 3rd, etc.
    bracketPlace    : int?         // 1 = winner, 2 = runner-up, etc.
    bestReactTime   : us?          // Fastest single reaction time across all bracket heats
    avgReactTime    : us?          // Average reaction time across all bracket heats
    bestInShowPlace : int?         // If awarded
}
```

### Heat Record

```text
HeatResult {
    id              : int
    sessionID       : int          // FK -> Session
    mode            : enum (TimeTrial, Bracket, Practice)
    heatSeq         : int          // Heat number within the event
    leftCarID       : int?         // FK -> Car; null if car not matched
    rightCarID      : int?
    leftCarTime     : us
    rightCarTime    : us
    leftReactTime   : us?          // null in GATEDROP mode
    rightReactTime  : us?
    leftFoul        : bool
    rightFoul       : bool
    winner          : enum (Left, Right, Tie, Void, Rerun)
    notes           : string?      // Operator edit notes
}
```

### Session Retention

A Session (see CONTEXT.md) is identified by date/time plus an optional operator-entered friendly name. Completed and abandoned sessions are **retained, never auto-deleted** -- storage is cheap and retention makes an accidental "End Race" or "New Race" recoverable by resuming the old session. A GUI screen for browsing and deleting old sessions is a low-priority backlog feature; until it exists, cleanup is a manual filesystem task on the Pi.

---

## Settings

Configurable per event (stored with the session, not globally):

| Setting                 | Default | Description                                   |
| ----------------------- | ------- | --------------------------------------------- |
| Time trial runs per car | 5       | How many Gate Drop runs are recorded per car  |
| Best in Show divisions  | 1       | 0 none, 1 single, 2 Younger/Older age groups  |

---

## Testing and Verification

Development is largely unattended (agents working GitHub Issues), so every work item needs machine-checkable proof. The stack mirrors the firmware's L2 desktop tests:

- **pytest suite** over all race logic: bracket generation and seeding, car matching, session save/resume, standings, time trial completion rules. No BLE, browser, or Pi required.
- **Race-day simulation**: the Mock Finish Controller replays a scripted full event (roster -> check-in -> time trials -> bracket -> race day complete) end-to-end; the test asserts the final database contents.
- **UI review is human**: screenshots and browser walkthroughs at review checkpoints. No browser-automation suite -- the UI stays thin, with all decisions in the tested backend.

To make the mock swappable for real hardware, the backend talks to the Finish Controller through a single link interface; the Mock Finish Controller and the real BLE central (`bleak`) are two implementations of it. Nothing above the link layer knows which one is connected.

---

## Implementation Phases

Development is mock-first: milestones M0-M7 run entirely on a development PC against the Mock Finish Controller, with pytest proof (see Testing and Verification). Firmware work (M8) waits for the AD architecture slices to land in main. After M4 the system can already run a real time-trial-only race night once M8/M9 exist -- a usable fallback checkpoint.

### M0 -- Contract and Scaffold

- [ ] Freeze GATT contract v1 on paper (UUIDs, characteristic table, payload structs) -- the shared reference for mock, app, and firmware
- [ ] Python project scaffold in `software/raceManager/` with pytest wired
- [ ] Retire the two pre-design stub scripts in `software/raceManager/src/`

### M1 -- Mock Finish Controller and Event Log

- [ ] FC link interface (the seam where mock and real BLE are interchangeable)
- [ ] Mock Finish Controller: replays scripted race states, Car IDs, and heat results
- [ ] Bare app that subscribes to the link and logs the event stream -- proves the data path

### M2 -- Session and Database Core

- [ ] SQLite schema: Racer, Car, Session, HeatResult (see Persistent Database)
- [ ] Session create / continuous save / resume; retention policy (never auto-delete)
- [ ] Power-on menu logic: Continue / New Race / Practice / Quit

### M3 -- Roster and Check-In (no RFID)

- [ ] Manual entry, CSV import with preview and duplicate flagging
- [ ] Previous-year roster import; grade prefill (last year + 1)
- [ ] Check-in marking; Settings UI (runs per car, Best in Show divisions)

### M4 -- Time Trial

- [ ] Time trial grid: runs, best, last, trend, completed-row styling
- [ ] Manual pre-select matching; solo runs (empty-lane DNF auto-discard)
- [ ] Full-runs warning and override; time edits with logging
- [ ] Places calculation (hidden until reveal); Practice mode (no DB writes)

### M5 -- Bracket

- [ ] Bracket generation: single elimination, seeding, byes to top seeds
- [ ] Bracket tree screen: current / on-deck / completed markings
- [ ] Foul rules (DQ, double-foul rerun), Bye/forfeit, manual winner override
- [ ] Staging / matchup reveal screen; car verification against the scheduled matchup

### M6 -- Race Day Complete

- [ ] Standings: time trial places, bracket places, best reaction time
- [ ] Celebration reveal screen (operator-advanced); Best in Show entry
- [ ] CSV export (PDF stretch)

### M7 -- Kiosk Deployment (any time after M2)

- [ ] Pi setup: auto-launch at boot (systemd or autostart), Chromium kiosk mode
- [ ] Quit to Desktop and Shutdown actions

### M8 -- Firmware BLE (hands-on; after AD slices land in main)

- [ ] Update branch from main once AD slices are merged
- [ ] GATT service and characteristics on the Nano 33 BLE per the frozen contract
- [ ] `reactionValidMask` in heat-result path (OQ-RM4)
- [ ] Set Mode / Initiate Test write guards (`RACE_IDLE` only); heartbeat
- [ ] Validate BLE notify timing against the display ISR (30 us digit settle)

### M9 -- Hardware Integration

- [ ] Real BLE link implementation (`bleak`) behind the M1 interface
- [ ] End-to-end bench test: two boards + Pi, full mock-scripted scenario replayed for real
- [ ] Empty-lane GATEDROP heat verified on the bench (solo-run support)

---

## Open Questions

| # | Question | Why it matters |
| --- | --- | --- |
| OQ-RM1 | ~~What application framework?~~ **Resolved:** Python backend + web UI in Chromium kiosk (see Technology Choice). | Drives all RPi application architecture |
| OQ-RM2 | RFID car ID format -- single byte, multi-byte UID, other? | Affects `BLEHeatResult` struct and GATT characteristic sizing; resolved when RFID branch is designed |
| OQ-RM3 | ~~Grade/age field: where and how?~~ **Resolved:** optional grade on the annual Car record (grade is a per-year fact); returning-roster import prefills last year's grade + 1. Needed for Best in Show division assignment. | Low urgency; doesn't affect race logic |
| OQ-RM4 | ~~How to signal reaction time "not applicable"?~~ **Resolved:** explicit `reactionValidMask` byte in `BLEHeatResult`; see Heat Result Payload. | Affects `BLEHeatResult` struct size and Race Manager parsing logic |
| OQ-RM5 | ~~Split Racer into persistent person + annual car?~~ **Resolved:** split per the glossary -- persistent `Racer` (person) + annual `Car` (carNumber, rfidCarID, year); heat results reference Cars. See Persistent Database. | Affects foreign key structure in `HeatResult` and `SeasonResult`; name display throughout UI |

---

## Dependencies and Blockers

| Item | Status | Blocks |
| --- | --- | --- |
| GATT contract defined on paper | Not started | Mock Finish Controller; all RPi application work |
| Mock Finish Controller (fake BLE peripheral emitting scripted race states and heat results) | Not started | RPi application development without firmware or track hardware |
| BLE GATT service implemented on FC firmware | Deferred until the AD architecture slices land in main and are pulled into this branch (avoids merge conflicts in finishController) | End-to-end hardware validation only -- NOT RPi development, which runs against the mock |
| Application framework decision (OQ-RM1) | Needs research | All RPi application coding |
| RFID firmware branch | Not started -- own branch; NOT a prerequisite (manual car matching is first-class) | RFID pairing at check-in only |
| `reactionValidMask` added to firmware (OQ-RM4) | Not started -- part of the firmware BLE milestone | Correct reaction-time validity in BLE payload |
