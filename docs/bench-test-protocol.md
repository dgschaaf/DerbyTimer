# Bench Test Protocol: Two-Board Integration

Standalone procedure for bringing up and verifying the Start Controller (SC)
and Finish Controller (FC) together on the bench. Written to be followed
cold -- no memory of past sessions assumed. Record every run in
`docs/test-log.md`.

Related docs: [testing.md](testing.md) (layer map) | [protocol.md](protocol.md)
(wire format) | [race-test-codes.md](race-test-codes.md) (self-test codes)

---

## 1. The diagnostic toolkit (read first)

**There are no breakpoints in this workflow.** Neither board supports
debugger-based development without extra probe hardware, and pausing one
controller mid-heat makes the other's 50 ms ACK timeouts and 10 s abort
timers fire -- you would be debugging the pause, not the system. Everything
below is observe-while-running.

Your three windows into the system, in order of usefulness:

1. **FC debug stream (primary).** Build the FC with `DERBY_DEBUG` and leave
   its USB plugged into the laptop with a serial monitor open at 115200.
   This is safe during live racing: the wire protocol runs on Serial1
   (D0/D1), debug text goes to USB. You will see every state transition,
   sensor finish, computed time, and winner mask as they happen.
2. **SC lights (secondary).** The SC runs a NORMAL build; its tree lights
   are its telemetry. Blue = staging, yellow sequence = countdown,
   green = GO, red patterns = errors (see race-test-codes.md).
3. **Protocol tap / simulator (when 1+2 are not enough).** The SH-U09C
   USB-TTL adapter (FTDI FT232RL) either watches the wire passively or
   *becomes* one of the controllers. Details in section 3.

**HARD RULE -- SC flashing:** the classic Nano's D0/D1 pins are shared
between the comm link and its onboard USB-serial bridge. **Disconnect the
comm cable from the SC's J5 before flashing or opening a USB serial monitor
on the SC.** Two drivers on the same pins can corrupt the upload and the
protocol. (The FC has no such restriction -- its USB is independent.)

### Build and flash commands

```sh
# FC with debug output (the bench workhorse):
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble --library firmware/lib/shared --build-property "build.extra_flags=-DDERBY_DEBUG" firmware/finishController/finishController.ino
arduino-cli upload  --fqbn arduino:mbed_nano:nano33ble -p <FC-COM-port> firmware/finishController/finishController.ino

# SC normal build (comm cable DISCONNECTED while uploading):
arduino-cli compile --fqbn arduino:avr:nano --library firmware/lib/shared firmware/startController/startController.ino
arduino-cli upload  --fqbn arduino:avr:nano -p <SC-COM-port> firmware/startController/startController.ino
```

VS Code tasks "Compile FC Debug" etc. in `.vscode/tasks.json` are
equivalents. Any serial monitor works for the FC debug stream (VS Code,
Arduino IDE, PuTTY, Docklight) at 115200.

---

## 2. Wiring reference

```
SC (5V TTL)                              FC board
J5 Comm  ----- comm cable (5V TTL) ----- J2 Comm --[level shifter]-- Nano33BLE D0/D1 (3.3V)
```

The cable itself carries **5 V TTL in both directions** -- the FC board's
level shifter sits between J2 and the Nano 33 BLE. Two signal lines:

- **SC-TX line** (SC D1 -> FC): carries SC->FC messages
- **FC-TX line** (FC -> SC D0): carries FC->SC messages

---

## 3. Using the SH-U09C FTDI adapter

Set the **voltage jumper to 5 V** for everything in this document (you only
ever connect at the cable / J5 / J2, which are all on the 5 V side of the
FC's level shifter). 3.3 V would only apply if probing directly at the
Nano 33 BLE's D0/D1 pins -- don't, use the connector side.

Never connect the adapter's VCC pin to the boards; they are self-powered.
GND must always be common.

### 3a. Passive tap (watch the live conversation)

- Adapter **GND** -> system GND
- Adapter **RX** -> the signal line you want to watch
- Adapter TX -> **leave unconnected**

One adapter watches ONE direction at a time. Tap the SC-TX line to see
SC->FC traffic (states, race start, reactions, fouls); tap the FC-TX line
to see FC->SC traffic (ACKs, winner, completion).

**Tool choice:**

- **First: `firmware/tools/uart_monitor.py`** -- it decodes DerbyTimer
  messages by name, which no generic terminal can do.

  ```sh
  pip install pyserial          # once
  python firmware/tools/uart_monitor.py COM5 --log bench-run.txt
  # then choose [1] Passive Monitor
  ```

- **Second: Docklight** (or any raw-hex terminal) -- when you suspect the
  decoder is hiding something: framing corruption, stray bytes, or timing
  gaps. Timestamped raw hex shows the wire exactly as it is. 115200 8N1.

### 3b. Simulator endpoint (laptop BECOMES the missing controller)

This is the most valuable pre-heat test: each real board gets a full
integration test against a scripted partner before the two boards ever meet.

Wiring (adapter replaces the absent board; note the cross-over):

- Adapter **TX** -> device RX line, Adapter **RX** -> device TX line,
  GND common. Against the real SC connect at J5; against the real FC
  connect at J2.

Run `python firmware/tools/uart_monitor.py <COM>` and pick:

- **[3] FC Simulator** against the **real SC**: the laptop ACKs everything
  and sends a winner on keypress. Walk the SC through a full heat with the
  buttons and watch its lights behave.
- **[2] SC Simulator** against the **real FC**: the laptop walks the state
  sequence and sends RACE_START; trigger the finish sensors by hand and
  confirm the FC computes/displays times and sends MSG_WINNER.
- **[4] Protocol Injector** for ad-hoc poking (send any message, observe
  the response; good for testing NACK/bounds-check behavior).

---

## 4. Staged bring-up checklist

Do the stages in order. Do not skip ahead past a failure -- each stage
assumes the previous one passed. Log each stage's result in test-log.md.

### Stage 0 -- Power-only smoke

Power each board (no comm cable). Expect: SC lights off after boot blip;
FC displays blank. No hot components, no magic smoke.

### Stage 1 -- L4 self-test, each board standalone

Hold the SC MODE button at power-up to enter RACE_TEST. Without the comm
cable, SC Phase 0 (FC ping) is EXPECTED to fail with one red blink + code
107 -- that is correct behavior, continue through the light chase, gate
cycle, and button phases. FC: run its functional checks via the hardware
test sketches in `hardware/hwTest/` if not already done at assembly.
Result codes: race-test-codes.md.

### Stage 2 -- Simulator integration, each board alone (section 3b)

- Real SC + laptop FC-sim: full GATEDROP and REACTION sequences from the
  buttons. Pass = SC lights walk the states and win lights fire on the
  simulated winner.
- Real FC + laptop SC-sim: scripted heat with hand-triggered sensors.
  Pass = FC debug stream shows `->RACING`, `left/right finish us=`,
  plausible times on the displays, `winner mask=` sent.

### Stage 3 -- Two-board link check

Comm cable connected, both boards powered, FC USB debug monitor open.
Hold SC MODE at power-up (RACE_TEST). Phase 0 pings the FC over the real
cable:

- **3 blue blinks** = link good both directions. Proceed.
- **1 red blink + code 107** = link dead. Check: TX/RX not crossed
  (SC TX must arrive at FC RX), GND common, cable seated at J5/J2.
  Tap each line per 3a to see which direction is silent.

### Stage 4 -- First GATEDROP heat

1. Both boards idle. FC debug monitor open.
2. SC: Start -> STAGING (blue lights; gates return). Load nothing yet.
3. Start again -> COUNTDOWN -> GO (gates drop).
4. Hand-trigger each finish sensor (block the beam) a second or two apart.
   Remember the 500 ms minimum-race-time filter: triggers in the first
   half second after GO are ignored by design.
5. Expected FC debug lines, in order:
   `[FC] ->STAGING`, `[FC] ->COUNTDOWN`, `[FC] ->RACING`,
   `[FC] left finish us=...`, `[FC] right finish us=...`,
   `[FC] ->COMPLETE`, `[FC] winner mask=...`
6. Expected hardware: FC displays show two plausible times; SC win lights
   blink for the faster lane; Start press advances/clears the displays;
   both boards return to IDLE (SC lights off, FC blank).
7. Repeat 3 heats. Times should be stable and sane.

### Stage 5 -- REACTION heat, foul, and abort recovery

1. SC: MODE button to REACTION (Y2 confirmation blink).
2. Clean heat: hold both lane buttons through GO, release after GO,
   hand-trigger the sensors. Expect reaction times in the FC debug stream
   and on the second display page (Start press advances to it).
3. **Foul heat (verifies the foul-reaction fix):** release ONE lane during
   the countdown (before GO). Expect: red foul light overlay on that side,
   `[FC] rx foul:` and a `react us=` line for the FOULED lane in the debug
   stream, fouled lane's display blank at COMPLETE, other lane wins.
4. **Abort recovery (verifies timeout paths):** start a heat and pull the
   comm cable during COUNTDOWN. Expect: SC's RACE_START times out ->
   SC returns to IDLE (3 red blinks at IDLE entry = sync warning); FC's
   10 s countdown timeout -> `[FC] countdown timeout->forceIdle`, displays
   blank. Reconnect the cable; the next heat must run normally with no
   power cycle.

### Stage 6 -- Record

Append a test-log.md row: date, stages run, pass/fail per stage, measured
oddities (time skew, retry noise in the tap log), firmware commit hash.

---

## 5. Symptom -> likely cause

| Symptom | Likely cause / first check |
| --- | --- |
| SC Phase-0 ping fails (code 107) | TX/RX crossed at J5/J2, missing GND, cable unseated. Tap each line (3a) to find the silent direction. |
| SC GO light goes dark seconds after GO, heat dies | Stale-state regression (serviceRx) -- check FC debug for an unexpected `->IDLE`; tap SC->FC line for a spurious MSG_RACE_STATE. |
| FC never arms / no finish times | MSG_RACE_START not arriving: watch FC debug at GO; tap the SC-TX line for ID 0x05 at GO. |
| Times look uniformly long/short by tens of ms | RACE_START retries delaying t0 -- tap log will show repeated 0x05 sends; check cable integrity. |
| Reaction times missing (car times = race times) | FC debug shows the missing-react warning; tap SC-TX for MSG_LEFT/RIGHT_REACT after GO; in GATEDROP mode this is normal. |
| Fouled lane shows a time instead of blank | Foul flag lost -- FC debug should show `rx foul:` at RACING entry; tap for MSG_FOUL (ID 0x09). |
| Garbage on the displays / NACK storms in the tap log | Wire corruption: shorten the cable, check GND, try a lower baud rate as a diagnostic. |
| SC won't flash / upload errors | Comm cable still connected to J5 -- disconnect it (hard rule, section 1). |
| One direction silent in taps but boards seem half-working | One conductor broken in the cable; ACKs vanish -> every message retries 4x then fails. |
