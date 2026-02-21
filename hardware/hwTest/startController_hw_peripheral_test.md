# Start Controller Hardware Peripheral Test

## 1. Objective

The objective of this test is to perform a basic checkout of the Start Controller PCB before software/firmware integration. This test checks all external peripheral connections, voltage regulation, button input circuits, gate control circuits with electromagnets, and solenoid functionality.

---

## 2. Background

The Start Controller board is part of the derby timer system and is designed to be a custom Arduino shield at the starting end of the race track.  Its major functions are:

- Managing four momentary pushbutton inputs (Start, Mode, Left trigger, Right trigger) with debouncing circuitry (74HC7014 Schmitt trigger buffer)
- Controlling gate electromagnets (Gate Left and Gate Right) for race gate operation
- Controlling a return solenoid for gate reset
- Interfacing with left and right Christmas tree light assemblies via shift register control (6 bulbs each)
- Interfacing with optional left and right RFID modules (future firmware integration)
- Communicate with the Finish Controller via logic-level-shifted UART serial communication
- Accept and condition +12V external power to +5V, and +3.3V power rails

This peripheral test validates basic board functionality at the component level before microprocessor integration.

---

## 3. Equipment Required

- Digital multimeter with DC voltage measurement capability
- External 12 VDC regulated power supply
- Four momentary pushbutton switches (SPST)
- Gate control assembly with two electromagnets and one return solenoid
- Left and right Christmas tree light assemblies (LED arrays)
- Jumper wires (22 AWG) for temporary connections
- Schematic printouts (optional, for reference during testing)

---

## 4. Setup and Configuration

### 4.1 Pre-Test Visual Inspection

1. Confirm PCB is fully mounted in its housing and all solder joints are clean and complete
2. Confirm PCB to enclosure connectors are properly seated and have continuity to enclosure connectors

### 4.2 Power Supply Setup

1. Set  12V power supply to **12.0V**
2. Enable current limiting feature on supply (if available)
3. Confirm channel output is **OFF** until directed in test procedure

### 4.3 Peripheral Connections

Connect all external peripherals to the Start Controller board as follows:

1. **Power Supply**: Connect external 12V power supply to connector **J6** (2-pin power input)
   - Pin 1: +12V
   - Pin 2: GND
2. **Gate Assembly**: Connect to connector **J2** (3-pin gate control)
   - Pin 1: +5V (enables electromagnet power)
   - Pin 2: GND
   - Pin 3: Solenoid discharge
3. **Left Starting Lights**: Connect to connector **J4** (3-pin light output)
4. **Right Starting Lights**: Connect to connector **J3** (3-pin light output)
5. **Button Triggers**: Connect to connectors **J7** through **J10** (2-pin each)
   - **J7**: Start trigger
   - **J8**: Mode trigger
   - **J9**: Left trigger
   - **J10**: Right trigger
   - Each button: one terminal to signal pin, other terminal to GND

---

## 5. Test Procedure

### 5.1 Power Supply Verification Test

**Objective**: Verify all voltage regulators produce output within specification.
**Test Steps**:

1. **SAFETY CHECKPOINT**: Before applyihg power, confirm
   - All connectors are properly seated
   - No visible solder bridges or defects
2. Turn **ON** 12V power supply output.  Immeadiately check and observe for:
   - Unusual noises or smells
   - Evidence of arcing or overheating
   - Dicoloroation or odor
3. Place multimeter ground probe on J12 pin 14 (GND reference)
4. Measure voltage on +12V rail:
   - Verify +12V LED indication is illuminated (D17)
   - Place test probe on J6 pin 1
   - Expected reading: **12.0V**, actual reading: _______ V
5. Measure voltage on +5V rail:
   - Verify +5V LED indication is illuminated (D19)
   - Place test probe on J2 pin 1
   - Expected reading: **5.0V**, actual reading: _______ V
6. Measure voltage on +3.3V rail:
   - Note: Due to design issue this version of the hardware uses an external power supply.
   - Place test probe on 3.3V regulator output
   - Expected reading: **3.3V**, actual reading: _______ V
7. If voltages are outside tolerance, STOP testing and investigate voltage regulator circuits

**Pass Criteria**: All three voltage measurements within specified tolerances

---

### 5.2 Button Input Test (Debounce Circuit Verification)

**Objective**: Verify button debouncing circuitry (74HC7014 Schmitt trigger) and input signal conditioning
**Test Steps**:

#### Test Start Trigger (J7)

1. Place multimeter ground probe on J12 pin 14 (GND reference)
2. Place test probe on J12 pin 11 (Start trigger output)
3. Confirm button is NOT pressed:
   - Expected reading: **0V (LOW)**, actual: _______ V
4. Press and hold Start trigger button
   - Verify Start Trigger LED indication is illuminated (D8)
   - Expected reading: **5V (HIGH)**, actual: _______ V
5. Release button:
   - Expected reading: **0V (LOW)**, actual: _______ V

#### Test Mode Trigger (J8)

1. Place multimeter ground probe on J12 pin 14 (GND reference)
2. Place test probe on J12 pin 10 (Mode trigger output)
3. Confirm button is NOT pressed:
   - Expected reading: **0V (LOW)**, actual: _______ V
4. Press and hold Mode trigger button
    - Verify Mode Trigger LED indication is illuminated (D6)
    - Expected reading: **5V (HIGH)**, actual: _______ V
5. Release button:
    - Expected reading: **0V (LOW)**, actual: _______ V

#### Test Right Trigger (J9)

1. Place multimeter ground probe on J12 pin 14 (GND reference)
2. Place test probe on J12 pin 9 (Right trigger output)
3. Confirm button is NOT pressed:
    - Expected reading: **0V (LOW)**, actual: _______ V
4. Press and hold Right trigger button
    - Verify Right Trigger LED indication is illuminated (D14)
    - Expected reading: **5V (HIGH)**, actual: _______ V
5. Release button:
    - Expected reading: **0V (LOW)**, actual: _______ V

#### Test Left Trigger (J10)

1. Place test probe on J12 pin 8 (Left trigger output)
2. Confirm button is NOT pressed:
    - Expected reading: **0V (LOW)**, actual: _______ V
3. Press and hold Left trigger button
    - Verify Left Trigger LED indication is illuminated (D12)
    - Expected reading: **5V (HIGH)**, actual: _______ V
4. Release button:
    - Expected reading: **0V (LOW)**, actual: _______ V

**Pass Criteria**: All four buttons transition cleanly between 0V and 5V with no floating or intermediate voltages.

---

### 5.3 Gate Electromagnet and Solenoid Control Test

**Objective**: Verify power distribution to gate electromagnets and solenoid circuits capable of energizing external loads
**Precautions**:

- Ensure gate assembly is properly mounted and electromagnets cannot cause injury
- Monitor electromagnet and solenoid temperatures during testing; do not allow to overheat
- Gate control circuits are designed to be driven by microprocessor GPIO, exercise caution when manually driving with jumper wires to avoid short circuits or damage

**Test Steps**:

#### Verify Gate Right Control Power

1. Place multimeter ground probe on J12 pin 14 (GND reference)
2. Place test probe on J12 pin 4 (Gate Right control output)
3. With no control signal applied:
   - Expected reading: **0V (floating)**, actual: _______ V
4. Apply +5V control signal using jumper wire from J2 pin 1 to J12 pin 4
5. Verify Gate Right electromagnet energizes (push gate into latched position)
6. With control signal applied, measure voltage:
   - Expected reading: **~2.7V to 5.0V**, actual: _______ V
7. Remove jumper wire - electromagnet should de-energize

#### Verify Gate Left Control Power

1. Place multimeter ground probe on J12 pin 14 (GND reference)
2. Place test probe on J12 pin 5 (Gate Left control output)
3. With no control signal applied:
    - Expected reading: **0V (floating)**, actual: _______ V
4. Apply +5V control signal using jumper wire from J2 pin 1 to J12 pin 5
5. Verify Gate Left electromagnet energizes (push gate into latched position)
6. With control signal applied, measure voltage:
    - Expected reading: **~2.7V to 5.0V**, actual: _______ V
7. Remove jumper wire - electromagnet should de-energize

#### Verify Solenoid Control Power

1. Place multimeter ground probe on J12 pin 14 (GND reference)
2. Place test probe on J12 pin 6 (Return solenoid control output)
3. With no control signal applied:
    - Expected reading: **0V (floating)**, actual: _______ V
4. Apply +5V control signal using jumper wire from J2 pin 1 to J12 pin 6
5. Verify return solenoid energizes (should hear/feel click or vibration)
6. With control signal applied, measure voltage:
    - Expected reading: **~2.7V to 5.0V**, actual: _______ V
7. Remove jumper wire - solenoid should de-energize
8. Remove all jumper wires

**Pass Criteria**: All three electromagnet/solenoid circuits show proper response to control signal; no audible arcing or component overheating

---

### 5.4 Light Control Test

**Objective**: Verify connections between PCB and light assemblies can drive bulbs.
**Test Steps**:

1. Place jumper wire on J6 pin 1 (GND reference)
2. **Safety Precaution**: Do not short jumper wire to J4 pin 1 or J3 pin 1 (+12V)
3. Place other end of jumper wire on J4 pin 2 (Blue Right)
4. Verify Blue Right bulb illuminates
5. Remove jumper wire and verify bulb turns off
6. Repeat for J4 pins 3-7 (Y3 Right, Y2 Right, Y1 Right, Green Right, Red Right)
7. Place jumper wire on J3 pin 2 (Blue Left)
8. Verify Blue Left bulb illuminates
9. Remove jumper wire and verify bulb turns off
10. Repeat for J3 pins 3-7 (Y3 Left, Y2 Left, Y1 Left, Green Left, Red Left)
11. Remove jumper wire from J6 pin 1

**Pass Criteria**: All 12 bulbs can be individually illuminated with jumper wire control; no bulbs remain dimly lit when jumper is removed, and no signs of arcing or overheating.

---

## 6. Teardown

1. Turn OFF the external 12V power supply
2. Remove all test equipment and jumper wires
3. Disconnect external peripherals (gate assembly, buttons, lights)
4. Visually inspect board for any damage or discoloration
5. Document any anomalies for rework

---

## 7. Test Results Summary

| Test Section | Status | Comments | Technician | Date |
| --- | --- | --- | --- | --- |
| 5.1 Power Supply | ☐ Pass ☐ Fail | | | |
| 5.2 Button Inputs | ☐ Pass ☐ Fail | | | |
| 5.3 Gate/Solenoid | ☐ Pass ☐ Fail | | | |
| 5.4 Light Control | ☐ Pass ☐ Fail | | | |

**Overall Status**: ☐ Pass ☐ Conditional ☐ Fail
**Notes / Action Items**:

---

---

---

---

## Appendix A: Connector Pinout Quick Reference

| Connector | Purpose | Pin 1 | Pin 2 | Pin 3 | Pin 4 | Pin 5| Pin 6 | Pin 7 | Pin 8 |
| --- | --- | --- | --- | --- | --- |
| **J1** | RFID L&R | +3.3V | GND | RST | SCK | MOSI | MISO | CS L | CS R |
| **J2** | Gate Control | +5V | +12V | Gate R Rtn | Gate L Rtn | Gate Reset Rtn | | | |
| **J3** | Tree Left | +12V | Blue | Y3 | Y2 | Y1 | Green | Red | |
| **J4** | Tree Right | +12V | Blue | Y3 | Y2 | Y1 | Green | Red | |
| **J5** | Comm | RXD | TXD | GND | | | | | |
| **J6** | Power | GND | +12V | | | | | | |
| **J7** | Trig Left | TrigL | GND | | | | | | |
| **J8** | Mode | Mode | GND | | | | | | |
| **J9** | Trig Start | TrigS | GND | | | | | | |
| **J10** | Trig Right | TrigR | GND | | | | | | |

## Appendix B: Troubleshooting Guide

TBD
