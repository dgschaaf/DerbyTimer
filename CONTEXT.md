# DerbyTimer

A two-controller embedded timing system for pinewood derby races, managing the full lifecycle of a heat from staging through finish line detection and result display.

## Language

**Heat**:
One complete run -- a single IDLE -> STAGING -> COUNTDOWN -> RACING -> COMPLETE -> IDLE cycle. The atomic unit of competition.
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
A gate drop that occurs before the GO signal in a reaction-mode heat. Disqualifies that lane -- no time is shown on the display and the lane cannot win. Not a time penalty.
_Avoid_: Early start, false start

**Run**:
One result attributed to a single car from a single heat, in the context of time trials. The car's viewpoint. One heat always produces exactly two runs (one per lane).
_Avoid_: Attempt, pass, heat (when describing a per-car result)

**Time Trial**:
The gate-drop phase of an event where each racer accumulates individual runs to establish their best time. Heats run in any order; the Race Manager matches results to racers by Car ID. Trophy is awarded to the fastest time trial car.
_Avoid_: Qualifying, practice rounds, seeding rounds

**Bracket**:
The single-elimination phase of an event run in Reaction or Pro Tree mode. All non-scratched cars enter. Seeded by time trial best times -- top-seeded cars receive byes as needed to fill the bracket to the next power of 2. Bragging rights go to the bracket winner.
_Avoid_: Elimination rounds, finals

**Car**:
The primary racing entity -- the physical vehicle that competes. Used everywhere in race results, brackets, time trials, lane assignments, and RFID identification. A car belongs to a specific racer for one event year.
_Avoid_: Racer (when referring to what's on the track or in results)

**Racer**:
The person behind the car. Used only when referring explicitly to name, personal identity, lifetime records, or club membership. A racer has a different car each year; the car is the annual entity, the racer is the persistent one.
_Avoid_: Car (when referring to the person's name or persistent identity)

**Trigger**:
A handheld momentary push-button on a cable (~6 ft), approximately the size of a can of pop. There are three triggers: Left (driver), Right (driver), and Starter (race official). Hardware-debounced via Schmitt triggers on the PCB shield. In the firmware the Left and Right triggers are referred to as "buttons" (`isLeftPressed()`, `isRightPressed()`); the Starter trigger is `isStartPressed()`. All three are press-and-release (no hold required).
_Avoid_: Button (in design discussions -- use Trigger for handheld controls and Mode Button for the panel control)

**Mode Button**:
A momentary push-button mounted on the enclosure shell. Cycles the race mode in IDLE (Gate Drop -> Reaction -> Pro Tree and back), and returns to IDLE from STAGING. Intentionally positioned inconveniently -- it is a software escape hatch, not a normal race-flow control. Operators are not expected to use it to abort a heat mid-run; the correct response to a "driver not ready" situation is to let the heat play out and have the Race Manager retroactively nullify it. In firmware referred to as `isModePressed()`.
_Avoid_: Mode trigger, abort button

**Starter**:
The race official who operates the Starter Trigger to advance the race state (IDLE -> STAGING -> COUNTDOWN). Distinct from the drivers, who each hold their own lane Trigger. Not a role defined in the firmware -- the firmware sees only button inputs, not who pressed them.

**Lane**:
One of the two physical track positions -- Left and Right. Permanent fixture names in the firmware (sensors, electromagnets, solenoids). Also used to refer to the car and racer occupying that position during a heat; the context makes the meaning clear.
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
A bracket heat that produced no valid result because both lanes fouled. The matchup is immediately returned to the front of the queue as "now racing." No database record is written until a valid result (one or fewer fouls) is produced. The Race Manager triggers a rerun when it receives a No Result signal from the firmware.
_Avoid_: Void (which is operator-initiated), re-race

**No Result**:
The firmware signal for a heat that produced no valid winner -- currently double-foul, and in future any aborted or interrupted heat. Transmitted as a distinct value in the `MSG_WINNER` payload (a new bitmask constant `winner_noResult`, separate from `winner_tie`). The Race Manager interprets No Result as a Rerun in bracket context or two void runs in time trial context.
_Avoid_: DNF (cars do complete the track), tie (which is a separate outcome for two clean cars finishing at equal times), incomplete

**Best in Show**:
A subjectively judged award for car design, creativity, craftsmanship, humor, or other qualities -- entirely independent of race results. Judged by an external panel (not the operator). Up to three places per division; 2nd and 3rd are optional. Operator enters winners manually after judging. Not calculated from race data.
_Avoid_: Design award, appearance award

**Division**:
An age grouping used exclusively for Best in Show awards -- not for race format or bracket structure. All cars race together regardless of division. Configured per event: zero, one, or two divisions. Current typical split when two divisions are used: Younger (3rd-5th grade) and Older (6th-8th grade).
_Avoid_: Age group, category (as a synonym for division)

**Dial-In** (DIALIIN):
A future race mode where the Race Manager applies a per-car handicap to stagger gate drops (or reaction triggers) so that all cars theoretically arrive at the finish line simultaneously. The Race Manager first accumulates each car's average time from gate-drop heats, then computes handicaps and controls the precise start timing for each lane. Intended as a pure reaction mode -- drivers still trigger their own gates, but the Race Manager staggers the GO signal per lane. Requires a hardware revision to support independent per-lane countdown light control (current hardware shares all yellow and green lights across both lanes). Not implemented in firmware beyond the mode stub; architecture must not preclude it.
_Avoid_: Handicap mode (use Dial-In), staggered start (use handicap)

**Handicap**:
A per-car time offset in Dial-In mode, calculated by the Race Manager from that car's average race time across time trials. Applied by staggering when each lane's start signal fires so that all cars are expected to reach the finish line simultaneously. Stored and calculated by the Race Manager; the firmware receives the resulting timing commands over BLE.
_Avoid_: Head start, time penalty, offset

## Relationships

- An **Event** contains many **Heats**, organized into **Time Trials** and a **Bracket**
- A **Heat** produces exactly one result (winner, finish times, fouls) and exactly two **Runs** (one per lane)
- A racer's time trial is complete when they have accumulated the configured number of **Runs** (default: 5)
- The firmware executes individual **Heats** in whatever mode is configured; it has no awareness of **Time Trial** vs **Bracket** -- that distinction belongs to the Race Manager

## Example dialogue

> **Dev:** "So when a car fouls in a bracket heat, does the Race Manager record the heat?"
> **Darren:** "No -- the fouling car is disqualified and the opponent wins. One foul is a valid result. But if both cars foul it's a rerun -- no result is recorded and the matchup goes back to the front of the queue."
> **Dev:** "Got it. And for time trials -- if Car #12 has done 3 of 5 runs and the kid's dad scratches them, what happens to those 3 runs?"
> **Darren:** "They stay in the database, the car just doesn't get seeded into the bracket."
> **Dev:** "And the Car Time on the display after a clean reaction-mode heat -- that's gate-drop to finish, not green-light to finish?"
> **Darren:** "Correct. Race Time is green to finish, Reaction Time is green to gate-drop, Car Time is gate-drop to finish. Car Time is what we care about -- it's the car's performance without the driver's reaction in it."

## Flagged ambiguities

- "race" is used in firmware naming (`raceState`, `raceMode`, `raceResults`) to mean **Heat** -- this is implementation naming only; use **Heat** in documentation and design discussions.
- Foul lanes on the track displays: **blank the foul lane on both the car-time and reaction-time screens** (option B, resolved). A foul is an automatic loss, so the time is irrelevant -- showing one invites petty "I was only 0.5 seconds early" arguments -- and the BCD hardware cannot label a number as "early jump time" vs. "reaction time," so any number shown for a DQ'd lane risks confusion with the clean lane's result. Foul coaching data (how early the driver jumped) belongs in the Race Manager UI where it can be labeled properly. The firmware's `displayReactionTimes()` must blank foul lanes, consistent with `displayCarTimes()`.
- "car" and "racer" are sometimes used interchangeably in casual speech -- resolved: **Car** is the primary entity in all race data; **Racer** is reserved for when the person's name or persistent identity is the subject.
- Dial-In mode requires per-lane countdown light control, which the current hardware does not support (all yellow and green lights share a single shift register output). Any firmware work on Dial-In must be preceded by a hardware revision. This constraint is not captured in the firmware or PCB docs yet.
- The lane displays use the MC14543B BCD-to-7-segment decoder, which can only output digits 0-9 and blank. No alphabetic characters (no "DNF", "ERR", "FLt", etc.) are possible. Display design choices are constrained to numbers and blank.
- A lane that does not cross the finish line within maxRaceTimeUs (10 s) is shown as 9.999 on the display. This is intentional: it is the maximum measurable time, provides positive operator confirmation that the heat has ended, and is the only option given the BCD hardware. The Race Manager must treat a received time of exactly maxRaceTimeUs as a DNF sentinel, not a real measurement.
