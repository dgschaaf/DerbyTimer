# Finish Controller Hardware Peripheral Test

Bench procedure; the KiCad schematic (`hardware/finishBoard/`) is authoritative for nets, and `docs/diagrams/finishBoardWiring.dio` is authoritative for enclosure wiring and connector names.

## 1. Purpose

Find two kinds of fault before the Nano is installed: PCB design or trace errors, and wiring or solder errors in the enclosure harness. The test checks the power wiring, the three supply rails, the UART idle levels, and both sensor input circuits. Display segments are not tested here. With no Nano, the display driver inputs float, so the display check is in `finishController_hw_functional_test.md`.

## 2. Configuration

- Board mounted in the enclosure. **No Nano 33 BLE in the socket.**
- Nothing plugged into X2 to X6 (no Start Board cable, no sensors, no displays).
- The bench supply connects only to the power cable that enters through the X1 cable gland.
- **Ground reference:** for every voltage reading, put the black meter probe on the bench supply negative (-) terminal.
- **Safety:** the power cable is cut from an AC extension cord. Make sure that no mains plug or receptacle remains on either end.

## 3. Equipment

- Digital multimeter (DC volts, resistance, continuity beep)
- 12 V bench supply with current limit
- One jumper wire with small probe clips
- Optional: two QS18 optical sensors with cables, for step 18

## 4. Probe Points

### External connectors

Each face is shown as you see it when you look at the outside of the enclosure. Before the first run, compare the numbers with the numbers molded into each connector insert.

```text
X2 START BOARD (4-pin)     X4 SENSORS (4-pin)       X3 DISPLAY PWR (3-pin)
mounted rotated 90 deg     key on the top         key on bottom
CCW, key on left

   [4]  [3]                   [1]  [4]                     [3]
   [1]  [2]                   [2]  [3]                  [2]   [1]

1 Tx      3 GND               1 GND     3 Lane 1        1 GND
2 +12V    4 Rx                2 +12V    4 Lane 2        2 +5V
                                                        3 +12V
```

- X1 POWER is a cable gland, not a connector. Its cable goes without a break from the bench supply to X2: +12V to X2 pin 2, GND to X2 pin 3. Thus X2 pin 2 is the supply +12V test point on the enclosure.
- **On X2, pins 2 (+12V) and 3 (GND) are next to each other. Never put a jumper on X2.**
- X5 and X6 (display data) are not used in this test.

### Internal points

These are inside the enclosure. No external connector carries them.

| Point | Signal |
| --- | --- |
| J8 pin 5 (Nano socket, analog side) | Lane 1 conditioned output (Nano A1) |
| J8 pin 4 | Lane 2 conditioned output (Nano A0) |
| Capacitor C6 (silkscreen label) | +3.3V rail. The board has no 3.3 V test point. |

## 5. Procedure

Do the steps in order. Write P or F in each Result blank. Write the reading where a value is asked for. If a step fails, stop and record the reading. Later steps assume that earlier steps passed.

### Part A - Power OFF

| Step | Action | Expected | Result |
| --- | --- | --- | --- |
| 1 | Look at the board and the wiring inside the enclosure. | Board is seated. No loose wire strands, no solder bridges. | ____ |
| 2 | Set the bench supply to 12.0 V and the current limit to 0.5 A. Set the output OFF. Do not connect the power cable yet. | Supply shows 12.0 V, output OFF. | ____ |
| 3 | Continuity: power cable +12V conductor (supply end) to X2 pin 2. Then the power cable GND conductor to X2 pin 3. | Beep on both. | ____ |
| 4 | Continuity: X2 pin 2 to X4 pin 2, and X2 pin 2 to X3 pin 3. | Beep on both (+12V reaches sensors and displays). | ____ |
| 5 | Continuity: X2 pin 3 to X4 pin 1, and X2 pin 3 to X3 pin 1. | Beep on both (GND reaches sensors and displays). | ____ |
| 6 | Resistance: X2 pin 2 to X2 pin 3. Wait for the reading to settle. | No beep, and more than 100 ohm. A low reading is a short. Do not apply power. | ______ ohm |

### Part B - Power ON

| Step | Action | Expected | Result |
| --- | --- | --- | --- |
| 7 | Connect the power cable to the supply (+12V conductor to +). Turn the output ON. Watch for 5 seconds. | Supply does not go into current limit, and the fan speed does not change. No smoke, smell, or hot parts. | ______ mA |
| 8 | Look at the rail LEDs on the board, if you can see them. | D17 (+12V), D15 (+5V), and D16 (+3.3V) are all on and steady. | ____ |
| 9 | Measure X2 pin 2. | 11.5 to 12.5 V | ______ V |
| 10 | Measure X3 pin 2. | 4.80 to 5.20 V (+5V rail) | ______ V |
| 11 | Measure X3 pin 3. | Same as step 9 (+12V to displays) | ______ V |
| 12 | Find the capacitor labeled C6 on the board silkscreen. Measure each of its two pads. | One pad reads 3.20 to 3.40 V (+3.3V rail). The other pad reads 0 V. | ______ V |
| 13 | Measure X2 pin 1 (Tx), then X2 pin 4 (Rx). | About 5 V on both. The level shifter pull-ups hold an idle UART line high. | ______ V / ______ V |

### Part C - Sensor inputs

A QS18 sensor output pulls its lane line to GND. The board pulls each lane line up to +5V through 1 k ohm, and an inverter (U3) gives 3.3 V at the Nano pin when the line is pulled low. Thus an open input reads 0 V at J8, and an active sensor reads 3.3 V. The test stimulus is a jumper from a lane pin to **GND** (X4 pin 1). It draws only 5 mA.

**Never connect +12V (X4 pin 2) to a lane pin.** A diode on each lane line connects it to the +5V rail. +12V on a lane pin goes through that diode into the +5V rail, which can damage every 5 V part on the board. If the supply current jumps or the fan speeds up, remove the jumper at once.

| Step | Action | Expected | Result |
| --- | --- | --- | --- |
| 14 | Measure X4 pin 2. | Same as step 9 (+12V to sensors) | ______ V |
| 15 | Measure X4 pin 3, then X4 pin 4. | About 5 V on both (pull-up, nothing connected) | ______ V / ______ V |
| 16a | **Check before you jumper.** Measure the two pins you will connect: X4 pin 3, then X4 pin 1. | X4 pin 3 reads about 5 V. X4 pin 1 reads 0 V. If either pin reads about 12 V, you are on the wrong pin. Stop. Do not put on the jumper. | ______ V / ______ V |
| 16 | Measure J8 pin 5 and J8 pin 4. Then put the jumper from X4 pin 3 to X4 pin 1 and measure both again. Remove the jumper. | Before jumper: 0 V on both. With jumper: J8 pin 5 = 3.2 to 3.4 V, J8 pin 4 stays 0 V. After jumper removed: 0 V. If J8 pin 4 goes high instead, Lane 1 and Lane 2 are swapped. Record which pin went high. | pin 5: ______ V, pin 4: ______ V |
| 17a | **Check before you jumper.** Measure X4 pin 4, then X4 pin 1. | X4 pin 4 reads about 5 V. X4 pin 1 reads 0 V. If either pin reads about 12 V, stop. Do not put on the jumper. | ______ V / ______ V |
| 17 | Do step 16 again with the jumper from X4 pin 4 to X4 pin 1. | With jumper: J8 pin 4 = 3.2 to 3.4 V, J8 pin 5 stays 0 V. | pin 4: ______ V, pin 5: ______ V |
| 18 | Optional. Connect the QS18 sensors to X4. Block the Lane 1 beam, then the Lane 2 beam. | J8 pin 5 (Lane 1) and then J8 pin 4 (Lane 2) read 3.3 V while the beam is blocked and 0 V while it is clear. If it is the opposite, the sensor is set to light-operate; the firmware needs dark-operate. Record the sensor model number. | ____ |

### Part D - Restore

| Step | Action | Expected | Result |
| --- | --- | --- | --- |
| 19 | Turn the supply output OFF. Remove the jumper and the sensors. | Rail LEDs go off. | ____ |
| 20 | Add one row to `docs/test-log.md`: date, steps passed (for example "15/20 PASS, step 16 FAIL"), and every reading that was out of range. | Row added. | ____ |
