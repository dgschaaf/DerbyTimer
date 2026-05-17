# DerbyTimer

A two-controller embedded timing system for pinewood derby races, managing the full lifecycle of a heat from staging through finish line detection and result display.

## Language

**Heat**:
One complete run — a single IDLE → STAGING → COUNTDOWN → RACING → COMPLETE → IDLE cycle. The atomic unit of competition.
_Avoid_: Race (as a synonym for heat), run

**Event**:
A full race night consisting of multiple heats across time trials and a bracket.
_Avoid_: Race night, tournament, derby (as a synonym for event)

**Race Time** (`raceTimeUs`):
Elapsed time from the GO signal to the car crossing the finish line. The race clock.
_Avoid_: Finish time, raw time

**Reaction Time** (`reactionTimeUs`):
Elapsed time from the GO signal to the gate dropping (the driver's response). Measured at the Start Controller.
_Avoid_: Response time, driver time

**Car Time** (`carTimeUs`):
Elapsed time from the gate dropping to the car crossing the finish line. The car's performance stripped of human factors. Equals Race Time minus Reaction Time for a clean start. The value shown on the display and used for winner determination. Not displayed for a disqualified lane.
_Avoid_: Adjusted time, corrected time

**Foul**:
A gate drop that occurs before the GO signal in a reaction-mode heat. Disqualifies that lane — no time is shown on the display and the lane cannot win. Not a time penalty.
_Avoid_: Early start, false start

**Run**:
One result attributed to a single car from a single heat, in the context of time trials. The car's viewpoint. One heat always produces exactly two runs (one per lane).
_Avoid_: Attempt, pass, heat (when describing a per-car result)

**Time Trial**:
The gate-drop phase of an event where each racer accumulates individual runs to establish their best time. Heats run in any order; the Race Manager matches results to racers by Car ID. Trophy is awarded to the fastest time trial car.
_Avoid_: Qualifying, practice rounds, seeding rounds

**Bracket**:
The single-elimination phase of an event run in Reaction or Pro Tree mode. All non-scratched cars enter. Seeded by time trial best times — top-seeded cars receive byes as needed to fill the bracket to the next power of 2. Bragging rights go to the bracket winner.
_Avoid_: Elimination rounds, finals

**Car**:
The primary racing entity — the physical vehicle that competes. Used everywhere in race results, brackets, time trials, lane assignments, and RFID identification. A car belongs to a specific racer for one event year.
_Avoid_: Racer (when referring to what's on the track or in results)

**Racer**:
The person behind the car. Used only when referring explicitly to name, personal identity, lifetime records, or club membership. A racer has a different car each year; the car is the annual entity, the racer is the persistent one.
_Avoid_: Car (when referring to the person's name or persistent identity)

**Lane**:
One of the two physical track positions — Left and Right. Permanent fixture names in the firmware (sensors, electromagnets, solenoids). Also used to refer to the car and racer occupying that position during a heat; the context makes the meaning clear.
_Avoid_: Track (as a synonym for lane), Lane 1 / Lane 2

**Matchup**:
A scheduled bracket pairing of two specific racers. A matchup becomes a heat when it runs on the track.
_Avoid_: Pairing, race (as a synonym for matchup), round

**Scratch**:
A racer officially withdrawn from further competition. Scratched racers are excluded from bracket seeding and generation. Their completed runs remain in the database. Operator-applied at any point before or during the bracket.
_Avoid_: Forfeit, dropout, DNS (as a permanent state)

**Bye**:
An automatic advancement in the bracket when a racer has no eligible opponent (e.g. odd bracket size, opponent scratched). The active racer advances without racing.

**Rerun**:
A bracket heat that produced no valid result because both lanes fouled. The matchup is immediately returned to the front of the queue as "now racing." No database record is written until a valid result (one or fewer fouls) is produced.
_Avoid_: Void (which is operator-initiated), re-race

**Best in Show**:
A subjectively judged award for car design, creativity, craftsmanship, humor, or other qualities — entirely independent of race results. Judged by an external panel (not the operator). Up to three places per division; 2nd and 3rd are optional. Operator enters winners manually after judging. Not calculated from race data.
_Avoid_: Design award, appearance award

**Division**:
An age grouping used exclusively for Best in Show awards — not for race format or bracket structure. All cars race together regardless of division. Configured per event: zero, one, or two divisions. Current typical split when two divisions are used: Younger (3rd–5th grade) and Older (6th–8th grade).
_Avoid_: Age group, category (as a synonym for division)

## Relationships

- An **Event** contains many **Heats**, organized into **Time Trials** and a **Bracket**
- A **Heat** produces exactly one result (winner, finish times, fouls) and exactly two **Runs** (one per lane)
- A racer's time trial is complete when they have accumulated the configured number of **Runs** (default: 5)
- The firmware executes individual **Heats** in whatever mode is configured; it has no awareness of **Time Trial** vs **Bracket** — that distinction belongs to the Race Manager

## Example dialogue

> **Dev:** "So when a car fouls in a bracket heat, does the Race Manager record the heat?"
> **Darren:** "No — the fouling car is disqualified and the opponent wins. One foul is a valid result. But if both cars foul it's a rerun — no result is recorded and the matchup goes back to the front of the queue."
> **Dev:** "Got it. And for time trials — if Car #12 has done 3 of 5 runs and the kid's dad scratches them, what happens to those 3 runs?"
> **Darren:** "They stay in the database, the car just doesn't get seeded into the bracket."
> **Dev:** "And the Car Time on the display after a clean reaction-mode heat — that's gate-drop to finish, not green-light to finish?"
> **Darren:** "Correct. Race Time is green to finish, Reaction Time is green to gate-drop, Car Time is gate-drop to finish. Car Time is what we care about — it's the car's performance without the driver's reaction in it."

## Flagged ambiguities

- "race" is used in firmware naming (`raceState`, `raceMode`, `raceResults`) to mean **Heat** — this is implementation naming only; use **Heat** in documentation and design discussions.
- "car" and "racer" are sometimes used interchangeably in casual speech — resolved: **Car** is the primary entity in all race data; **Racer** is reserved for when the person's name or persistent identity is the subject.
