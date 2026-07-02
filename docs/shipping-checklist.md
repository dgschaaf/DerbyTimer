# DerbyTimer v1.0 Shipping Checklist

Work through these sections in order. Each section can be checked off independently as hardware becomes available. Serial protocol correctness (fwTest) is a prerequisite for the hardware integration section and is tracked separately.

---

## 1. Firmware -- Compile Gate

Do this at your desk before touching hardware.

- [ ] `startController` compiles clean, zero warnings
- [ ] `finishController` compiles clean, zero warnings
- [ ] All critical and high-priority items in the project backlog are resolved or explicitly accepted
- [ ] Fresh flash to both controllers from the same build

---

## 2. Bench -- Sensor Verification

Wire one SE61 sensor to A0 or A1 on the FC. Power the sensor. Confirm with serial monitor.

- [ ] `armSensors()` attaches interrupt without error
- [ ] Physically break the beam -> `isLeftFinished()` / `isRightFinished()` returns true
- [ ] Break beam immediately at arm time -> confirm 500 ms filter suppresses it (flag stays false)
- [ ] Confirm `config.activeHigh = true` matches your sensor wiring (beam-break = pin HIGH)
- [ ] Both lanes independently verified

---

## 3. Bench -- Display Verification

Connect one MC14543B + 74HC238 + display assembly. Power it. Confirm visually.

- [ ] All five digit positions illuminate correctly (no stuck segments, no ghosting between digits)
- [ ] Decimal point appears between digit 1 and digit 2 (ones and tenths -- e.g., `X.XXX`)
- [ ] `0b1111` BCD input blanks the digit cleanly -- no partial segments lit
  - If it does NOT blank: use the blanking pin (BI/RBO) instead and update `writeBlankDigit()`
- [ ] 30 us settle time is adequate -- digits don't bleed into adjacent positions at speed
- [ ] Repeat for the second lane display
- [ ] Error sentinel `88.888` displays correctly (all eights)

---

## 4. Integration -- Controllers Talking (prerequisite: fwTest complete)

Two controllers wired over UART, all peripherals connected. No track yet.

- [ ] IDLE -> STAGING: blue lights on SC, gates return, displays clear
- [ ] STAGING -> IDLE via Mode button: lights off, gates stay returned
- [ ] STAGING -> COUNTDOWN blocked until both gates confirm up (`areLanesReady()`)
- [ ] COUNTDOWN: Y3 -> Y2 -> Y1 -> GO sequence fires at correct intervals (500/500/500 ms)
- [ ] PRO mode: all three yellows light simultaneously on Y1 stage (400 ms single stage)
- [ ] Mode indicator blinks 3x in IDLE when mode changes
- [ ] GATEDROP: gates drop at GO; FC arms sensors; heat runs to completion
- [ ] Winner announced: left-win, right-win, and tie light patterns all verified
- [ ] Display advance (Start press in COMPLETE) cycles to reaction times, then IDLE
- [ ] Double-foul: both fouls sent -> `winner_noResult` -> no win lights, IDLE on next Start press
- [ ] One DNF (block one sensor for 10+ seconds): correct lane blanked, other lane displays time

---

## 5. Integration -- Fault Recovery

These verify the abort paths hold up on real hardware.

- [ ] Unplug UART cable during COUNTDOWN -> FC 10 s timeout fires -> both controllers return to IDLE independently
- [ ] Re-plug cable, run a normal heat -> system recovers cleanly with no stale state
- [ ] Power-cycle SC mid-heat -> FC eventually times out or DNFs -> operator can restart

---

## 6. Race Mode Sign-Off

Run at least one complete heat in each mode, end to end.

- [ ] MODE_GATEDROP -- both cars finish, winner correct
- [ ] MODE_REACTION -- car times displayed, reaction times on second Start press
- [ ] MODE_PRO -- single-stage countdown, car times displayed, reaction times on second Start press
- [ ] REACTION foul: one car triggers before GO -> red light on that lane -> foul lane blanked, other lane wins

---

## 7. Physical / Electrical

- [ ] All connectors crimped and fully seated
- [ ] UART cable run at full race-track length -- confirm no serial errors at 115,200 baud
- [ ] Gate electromagnets drop cleanly on de-energize (gravity + spring, no rebound catch)
- [ ] Return solenoid returns both gates within the 500 ms window
- [ ] Power supply adequate under full load: both controllers + gates + lights + displays simultaneously
- [ ] No shorts or heat issues after 30 min of continuous operation

---

## 8. Race-Day Readiness

- [ ] Fresh firmware flash the day before (eliminates "which version is on there?" questions)
- [ ] Full dry run: 4-6 back-to-back heats across multiple modes, no errors
- [ ] Laptop on-site with `arduino-cli` and both `.ino` files ready for emergency reflash
- [ ] USB cables for both Nanos packed
- [ ] Project backlog reviewed -- no surprise open high-priority items

---

## Known Accepted Limitations for v1.0

These are documented decisions, not forgotten items:

- **No software COUNTDOWN abort** -- Mode button is not checked during countdown. Recovery is to let the heat DNF naturally. Power cycle recovers a truly hung countdown (not yet observed).
- **No Race Manager** -- Win/loss is read from the FC display and lights. Bracket tracking is manual.
- **No RFID** -- Car identification is visual. Lane assignment is by physical position.
- **Display resolution is 1 ms** -- Sub-millisecond timing differences are rounded. This is typical for pinewood derby.
