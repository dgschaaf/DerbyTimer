# Track Integration Test (Temporary Wiring)

Bench procedure for the first full-system run on the track with temporary
wiring; the firmware and the wiring diagrams in `docs/diagrams/` are
authoritative.

**Goal:** get ready for the full race test, which is the real proof. This
run exercises what a race does not show clearly: the self-test, sensor
polarity, every display segment, the operator controls, and the edge cases
(fouls, DNF, abort). The track can be in pieces on the floor. A hand
through a beam stands in for a car.

Related: [race-test-codes.md](race-test-codes.md) |
[shipping-checklist.md](shipping-checklist.md) |
[bench-test-protocol.md](bench-test-protocol.md)

## Before you start

- Both boards run the normal build. Firmware commit: __________
- SC USB stays unplugged. The classic Nano shares D0/D1 between USB and
  the inter-board link. The FC USB is free.
- No return solenoid: lift the gates onto the magnets by hand.
- Turn the power off to rewire. Measure a power source before you jumper
  to it. Stop at the first FAIL and write down what you saw.

Bring: both enclosures with Nanos, inter-board cable, FC 12 V supply, buck
converter and its supply, multimeter, jumpers, tape, hand tools, this
sheet, race-test-codes.md, both board wiring diagrams. Laptop and
USB-to-TTL cable only for part 6.

## 1. Set up and power on

| # | Do | Expect | Result |
| - | -- | ------ | ------ |
| 1 | Seat both Nanos. Compare the Nano pin labels with the silkscreen at both ends of the header. Connect the inter-board cable. | Labels match, no bent pins. | SC __ FC __ |
| 2 | Buck on, nothing on its output. Measure it. Buck off. | 4.9-5.3 V. Above 5.5 V, stop: the gate-return diode would feed it into the SC 5 V rail. | ____ V |
| 3 | Check continuity from buck (-) to SC GND. | Beep. Without it the magnets never hold. | ____ |
| 4 | Wire buck (+) (yellow) to both magnets, magnet returns to L_rtn (red) and R_rtn (purple). Tape green, grey, blue. Jumper the SC buttons, the FC sensors and the displays, pins per the wiring diagrams. Leave display power off. | All connected; spare wires taped. | ____ |
| 5 | FC supply on. Buck still off. | Power LEDs on both boards. Tree dark after a boot blip. | ____ |
| 6 | Measure the display power source and compare it with the display requirement. Power off, connect the jumper, power on. | Values match. Displays blank. | ____ V |
| 7 | Buck on. Lift a gate onto its magnet. | It does not hold (magnets are off in IDLE). | ____ |

## 2. Self-test from the start end (RACE_TEST run A)

You stay at the start, so FC codes 201 and 202 are expected.

| # | Do | Expect | Result |
| - | -- | ------ | ------ |
| 8 | Hold Mode, press and release the SC reset button, release Mode. | Blue blinks 3 times: link OK both ways. One red blink (code 107): stop. | ____ |
| 9 | Watch the light chase. Hold both gates on their magnets before it ends. | All 8 lights. Then the left gate drops, then the right. | ____ |
| 10 | On each GO blink, press and release one button: Start, Mode, Left, Right (5 s each). | A confirm blink after each. Then GO blinks 5 times and stays on. | S _ M _ L _ R _ |
| 11 | Walk to the FC. Watch several cycles. | Both lanes cycle `00.201`, `00.202`, `88.888` (`00.203` = FC did not hear the SC: stop). On `88.888` every segment and decimal point is lit; in the blank, nothing is lit. | L __ R __ |
| 12 | Power-cycle the FC supply. | Both boards in IDLE. | ____ |

## 3. Self-test from the finish end (RACE_TEST run B)

You leave the start at once, so SC codes 103-106 are expected.

| # | Do | Expect | Result |
| - | -- | ------ | ------ |
| 13 | Start RACE_TEST as in step 8 and walk straight to the FC. The display test takes about 4 s, then the sensor window is open for about 8 s. | Both lanes count up. Hands off for 2 s: neither lane freezes. A lane that freezes by itself has the wrong sensor polarity. | ____ |
| 14 | Hand through the left beam, then the right. | Each lane freezes when its beam breaks. Then both show `00.000`. If the window closes first, repeat 13. | L __ R __ |
| 15 | Power-cycle the FC supply. | IDLE. | ____ |

## 4. Operator controls

| # | Do | Expect | Result |
| - | -- | ------ | ------ |
| 16 | Press and release Mode 3 times, slowly. | Y2 (Reaction), Y3 (Pro), Y1 (Gate Drop), 3 blinks each. Ends in Gate Drop. | ____ |
| 17 | Hold Start 3 s, then release. | Nothing while held. On release: STAGING, blue lights and magnets on. | ____ |
| 18 | Hold Mode 3 s, then release. | One change to IDLE. The mode does not change (no yellow blink). | ____ |

## 5. Heats

Each heat: Start for STAGING, lift both gates, Start for COUNTDOWN, walk to
the finish. Break the beams **within 10 s of GO**; after 10 s an unbroken
lane is a DNF. Read the times, walk back, and press Start to step the
displays (times, then reaction times, then IDLE).

Win lights: left wins = GO + right red, right wins = GO + left red (3
blinks, then steady). Tie = GO only.

| # | Do | Expect | Result |
| - | -- | ------ | ------ |
| 19 | Gate Drop. Break the left beam, then the right. | Y3, Y2, Y1 at 0.5 s steps, GO, both gates drop. Times plausible. Left wins. | L ____ R ____ |
| 20 | Gate Drop again, right beam first. Press Start through the displays. | Right wins. Displays: times, then blank reaction times, then IDLE. | ____ |
| 21 | Reaction (Y2). After GO press Left, then Right, then go break the beams. | Gates stay up at GO; each drops on its button. Reaction times show on the second Start press. | ____ |
| 22 | Reaction foul: press Left before GO. | Left gate drops, left red on. Left lane blank. Right wins. | ____ |
| 23 | Double foul: press both before GO. | No win lights. The next Start goes to IDLE. | ____ |
| 24 | Pro (Y3). | One 0.4 s yellow stage, then GO. Write which yellows light. | ______ |
| 25 | DNF: Gate Drop, break the left beam only. | After 10 s: left time, right blank, left wins. | ____ |
| 26 | Abort: start a heat, press Mode during the yellows. | Tree dark, gates drop, no red blink, IDLE. | ____ |
| 27 | 4-6 heats back to back, mixed modes. Watch the SC at each gate drop. | No resets, no unexpected red blinks. | ____ heats |
| 28 | Touch the buck, the regulators and the magnets. | Warm at most. | ____ |

## 6. Optional: capture logs for analysis at home

Do this only after part 5 passes. Save the files and bring them back.

**A. FC debug log over USB (no extra wiring).** Connect the laptop to the
FC USB. Run the task `Upload FC Debug`, then in the VS Code terminal
(PowerShell; use the FC COM port from `List Connected Boards`):

```
arduino-cli monitor -p COM5 -c baudrate=115200 | Tee-Object -FilePath fc-debug.txt
```

Run 2-3 heats, then Ctrl+C. If nothing appears, run it without the
`| Tee-Object ...` part and copy the terminal text into a file. Before you
leave, run `Upload Finish Controller` to restore the normal build.

**B. Wire tap with the USB-to-TTL cable.** First plug the cable into the
laptop with its leads free and measure each lead to black (GND). The
transmit lead idles high: write its voltage. The link is 5 V; if the lead
reads 3.3 V, skip this tap until the cable's chip is known. Otherwise
connect only GND and the receive lead (usually white) to the SC-TX line
(wiring diagram). Leave the cable's red (power) lead unconnected. Then:

```
pip install --user pyserial
python firmware/tools/uart_monitor.py COM6 --log bus-sc-tx.txt
```

Choose `[1] Passive Monitor`. Details: bench-test-protocol.md section 3.

## When a step fails

| Symptom | First check |
| ------- | ----------- |
| Code 107, or `00.203` at the FC | Inter-board cable and Nano seating |
| Magnets never hold in STAGING | Step 3 ground; buck on |
| SC resets (tree blips to IDLE) at a gate drop | Magnet turn-off energy into the SC 5 V rail. Note it; stop racing |
| Display digit dim, missing or ghosting | Display ribbon or jumper; note which digit |
| A lane never times (always DNF) | That sensor's wiring or polarity (step 13) |
| Wrong lane wins | Lane swap in the sensor or display wiring |
