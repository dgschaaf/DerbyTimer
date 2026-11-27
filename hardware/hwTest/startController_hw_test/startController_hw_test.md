# Start Controller Light-Path and Button Test

Bench procedure. The sketch `startController_hw_test.ino` in this folder is authoritative for pins and step patterns; the PCB file is authoritative for part designators.

## 1. Purpose

Isolate a dark or misbehaving christmas tree to one circuit, one part, or one connection, without the race firmware, the Finish Controller, or the serial link. The test answers five questions in order:

1. Does the SC Nano run code? (heartbeat LED, boot banner)
2. Does the 74HC595 (U4) drive each output? (one lamp per step, meter on U4 pins)
3. Does the "GO and not red" logic (U3, U1) work on each side? (GO + red steps)
4. Do the four buttons reach the Nano? (serial PRESSED / released lines)
5. Does the race firmware enter RACE_TEST when Mode is held at reset? (SC debug build, section 8)

Measure only. This procedure never connects a jumper or applies a stimulus.

## 2. Light path (from the PCB netlist)

```
Nano D2/D4/D3 --> U4 74HC595 --> Q0..Q7 --> 10k pull-down + DMG3402L switch per lamp per tree
                                  Q5 (GO) --> U3 74LVC2G08 (GO AND not-red, one gate per side)
                     Q6/Q7 (reds) --> U1 74LVC2G14 (inverter) --> U3 not-red inputs
```

| U4 output | U4 pin | Firmware name | PCB net | Lamp |
| --------- | ------ | ------------- | ------- | ---- |
| Q0 | 15 | BR | Blue R | none fitted |
| Q1 | 1 | BL | Blue L | none fitted |
| Q2 | 2 | Y3 | Yellow 1 | one yellow, both trees |
| Q3 | 3 | Y2 | Yellow 2 | one yellow, both trees |
| Q4 | 4 | Y1 | Yellow 3 | one yellow, both trees |
| Q5 | 5 | GO | Go | GO lamp (blue lens), both trees, through U3 |
| Q6 | 6 | FL | Red L | red, left tree |
| Q7 | 7 | FR | Red R | red, right tree |

The firmware and PCB names for Q2 and Q4 are reversed. Record which physical yellow lights at each step.

Other U4 pins: 16 = +5V, 8 = GND, 10 = reset (must be HIGH), 13 = output enable (must be LOW), 14 = data (D2), 11 = clock (D4), 12 = latch (D3).

U3 (SSOP-8, pin 1 at the dot): 1 = not-Red-R, 2 = Go, 3 = Green L, 4 = GND, 5 = not-Red-L, 6 = Go, 7 = Green R, 8 = +5V.
Green L = pin 5 AND pin 6. Green R = pin 1 AND pin 2.

U1 (SC-70-6): 1 = Red L in, 2 = GND, 3 = Red R in, 4 = not-Red-R out, 5 = +5V, 6 = not-Red-L out.

Gate nodes of the two GO lamp switches (measure at the end that is not GND):
R16 = Green L (Q6 gate), R22 = Green R (Q12 gate). R34 and R40 are on the lamp side, not the gate.

The Go net has no pull-down. If U4 does not drive pin 5, Go floats and the GO lamps can glow dimly or turn on after a delay.

## 3. Equipment

- Laptop with the repo pulled, arduino-cli installed, and a USB mini-B cable
- Multimeter (DC volts and ohms), fine probe tips
- Normal 12 V power to the SC (the lamps need it; USB alone powers only the Nano)
- Magnifier or phone camera zoom for solder inspection

## 4. Safety

- **Unplug the comm cable at J5 before you connect USB.** D0/D1 are shared between USB and the link.
- Probe resistor pads or the U4 legs (1.27 mm pitch). U3 and U1 pins are very close together: inspect them first, probe them last, and keep the probe still.
- Black lead on the Nano GND pin for every voltage reading.

## 5. Flash and first signs

1. Unplug J5. Connect USB. Run the VS Code task **List Connected Boards** and note the COM port.
2. Run the task **Upload SC Light-Path Test (J5 unplugged)**. Success: `Sketch uses ...` and no `Error` line. The build date in step 4 is the proof that the new code is on the chip.
3. Run the task **Serial Monitor 115200**. Opening the monitor resets the Nano.
4. Expected banner:
   ```
   === SC light-path test ===
   Built <date> <time>
   Mode at boot: raw=1023 -> released
   Commands: p = pause/resume, n = next, b = back, h = help
   [t=0s] step 0 0x00  ALL OFF ...
   ```

| Check | Pass | If it fails |
| ----- | ---- | ----------- |
| Banner prints once | Yes | Repeating banner = the Nano resets. Check the 5 V supply under lamp load. |
| Nano "L" LED blinks about once per second | Yes | No blink with no banner = the sketch does not run. |
| Built date/time is today | Yes | Old date = the upload did not happen. Re-run step 2. |

## 6. Light sequence

The sketch steps every 1.5 s through the table below, then repeats. Type `p` and Enter to pause on a step (the outputs stay latched), `n` / `b` to step.

| Step | Pattern | Expected lamps |
| ---- | ------- | -------------- |
| 0 | ALL OFF | All dark |
| 1, 2 | Blue R, Blue L | All dark (no blue lamps fitted) |
| 3, 4, 5 | Q2, Q3, Q4 | One yellow per step, same position on both trees |
| 6 | GO | GO lamp on both trees |
| 7 | Red L | Left red only |
| 8 | Red R | Right red only |
| 9 | GO + Red L | Left red ON, left GO OFF, right GO ON |
| 10 | GO + Red R | Right red ON, right GO OFF, left GO ON |
| 11 | ALL ON | Yellows and reds ON, both GO OFF (red suppresses GO) |

Record any step that does not match. Steps 3 to 8 test U4 and the lamp switches. Steps 6, 9, 10 and 11 test U3 and U1.

## 7. Probe readings (pause on the step first)

| Pause on step | Probe | Expect |
| ------------- | ----- | ------ |
| any | U4 pin 16 | 5 V |
| any | U4 pin 8, pin 13 | 0 V |
| any | U4 pin 10 | 5 V |
| any | U4 pin 12 (latch) | 5 V steady (proves the sketch wrote U4) |
| 0 | U4 pin 5 (Go) | 0 V, steady |
| 0 | R16, R22 gate ends | 0 V |
| 6 | U4 pin 5 | 5 V |
| 6 | U3 pins 2 and 6 | 5 V |
| 6 | U3 pins 1 and 5 | 5 V |
| 6 | R16 and R22 gate ends | 5 V |
| 9 | U1 pin 6 and U3 pin 5 | 0 V |
| 9 | R16 / R22 | 0 V / 5 V |
| 10 | U1 pin 4 and U3 pin 1 | 0 V |
| 10 | R16 / R22 | 5 V / 0 V |
| 3 to 8 | the U4 pin for that step (section 2 table) | 5 V; all other Q pins 0 V |

A reading that wanders, or sits between about 0.5 V and 4 V, marks a node that nothing drives.

Power-off continuity (ohms), for the hand-wired U3 repair:

| From | To | Expect |
| ---- | -- | ------ |
| U1 pin 6 | U3 pin 5 | about 0 ohm |
| U1 pin 4 | U3 pin 1 | about 0 ohm |
| U4 pin 5 | U3 pins 2 and 6 | about 0 ohm |
| Nano D2 / D4 / D3 | U4 pin 14 / 11 / 12 | about 0 ohm |

## 8. Buttons and Mode at boot

1. Press and release each button. Expect one line per change, for example `BUTTON Mode (A6) PRESSED  raw=3`, then `released`. The raw value is about 0 pressed and about 1023 released.
2. Hold Mode, press and release the Nano reset button, keep holding about 3 s. Expect `Mode at boot: raw=<low> -> PRESSED`. This is the same read the race firmware makes at power-up.

## 9. Race firmware: does it enter RACE_TEST?

1. J5 still unplugged. Run the task **Upload SC Debug (J5 unplugged)**, then **Serial Monitor 115200**.
2. Normal boot prints `[SC] ->IDLE`.
3. Hold Mode, press and release reset, keep holding about 3 s. Expect `[SC] ->RACE_TEST`, then the light chase on the tree. Unreadable characters after it are the protocol frames for the phase 0 ping, sent to USB because J5 is unplugged; phase 0 then fails with code 107, as expected here.
4. Run the task **Upload Start Controller (J5 unplugged)** to put the normal race firmware back. Close the monitor and reconnect J5.
5. With the FC powered, hold Mode and reset the SC. The FC display shows the test pattern (88.888) only if the SC ping crossed the link. No 88.888 here, with section 9 step 3 passed, isolates the fault to the link or the FC receive path.

## 10. Fault isolation

| Observation | Fault is in | Next action |
| ----------- | ----------- | ----------- |
| No banner, no L blink | SC Nano or its supply | Check 5 V at the Nano; reseat the Nano; reflash |
| Banner repeats | Nano resets | Measure 5 V while the lamps switch |
| Banner OK, no lamp at any step, U4 pin 12 at 5 V | U4 or its supply | Section 7 supply rows; reflow or replace U4 (SOIC-16) |
| Banner OK, U4 pin 12 not 5 V | D3 trace or joint | Continuity Nano D3 to U4 pin 12 |
| One Q pin stays 0 V on its step | U4 output | Replace U4 |
| U4 pin correct, one lamp dark | That lamp switch, its lamp, or the tree wiring | Compare the switch gate and the lamp low side with a working lamp |
| GO correct at U4 pin 5, one GO lamp wrong | U3, U1, or the repair wires | Section 7 rows for steps 6, 9, 10; continuity table |
| Lamps correct here, race firmware tree dark | Firmware or entry logic | Section 9 |
| Section 9 step 3 passes, no 88.888 in step 5 | Link or FC receive | Tap the link lines per docs/bench-test-protocol.md section 3a |
| Mode at boot reads released while held | Mode input path | Measure at A6 during the reset |

## 11. Results

| Item | Result | Notes |
| ---- | ------ | ----- |
| Banner, single, current build date | | |
| L LED heartbeat | | |
| Steps 3-5: physical yellow position for Q2 / Q3 / Q4 | | |
| Step 6 GO both trees | | |
| Steps 7, 8 reds | | |
| Steps 9, 10, 11 GO suppression | | |
| Section 7 readings out of range | | |
| Buttons: Start / Mode / Left / Right | | |
| Mode at boot PRESSED | | |
| Debug build prints ->RACE_TEST | | |
| FC shows 88.888 with link connected | | |

Log the run in docs/test-log.md.
