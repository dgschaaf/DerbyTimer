# Finish Controller Hardware Peripheral Test

## 1. Objective

The objective of this test is to perform basic checkout of the Finish Controller PCB before software/firmware integration. This test checks all external connectors, all voltage regulators, and sensor input signal conditioning circuits.

---

## 2. Background

The Finish Controller board is part of the derby timer system and is designed to be a custom Arduino shield at the finish end of the race track.  Its major functions are:

- Accept & amplify optical sensor inputs from two independent World Beam QS-18 series optical sensors (Lane 1 and Lane 2)
- Provide time-display output signals that drive two 5-digit 7-segment LED digit displays with multiplexed selection
- Accept and condition +12V external power to +5V, and +3.3V power rails
- Communicate with the Start Controller via logic-level-shifted UART serial communication
- Communicate with the Race Manager controller via BLE interface (future firmware integration)

This peripheral test validates basic board functionality at the component level before microprocessor integration.

---

## 3. Equipment Required

- Digital multimeter with DC voltage measurement capability
- External 12VDC regulated power supply
- Pair of optical sensors (World Beam QS-18 series) for stimulation testing
- Jumper wires (22 AWG) for temporary connections
- Schematic printouts (optional, for reference during testing)

---

## 4. Setup and Configuration

### 4.1 Pre-Test Visual Inspection

1. Confirm PCB is fully mounted in its housing and all solder joints are clean and complete
2. Confirm PCB to enclosure connectors are properly seated and have continuity to enclosure connectors

### 4.2 Power Supply Configuration

1. Set  12V power supply to **12.0V**
2. Enable current limiting feature on supply (if available)
3. Confirm channel output is **OFF** until directed in test procedure

### 4.3 Peripheral Connection

Connect all external peripherals to the Finish Controller board as follows:

1. **Power Supply**: Connect external 12V power supply to connector **J3** (2-pin power input)
   - Pin 1: +12V
   - Pin 2: GND
2. **SENSOR INPUT**: Connect to connector **J1** (3-pin sensor input)
   - Pin 1: +12V reference (internal supply to sensors)
   - Pin 2: Lane 1 digital output (to be stimulated with optical sensor)
   - Pin 3: Lane 2 digital output (to be stimulated with optical sensor)
   - Pin 4: GND reference
3. **DISPLAY POWER**: Connect to connector **J6** (4-pin display power)
   - Pin 1: +12V (for LED display back-light, if present)
   - Pin 2: GND
   - Pin 3: GND (additional ground for display)
   - Pin 4: +5V (for display segment driver logic)

---

## 5. Test Procedure

### 5.1 Power Supply Verification Test

**Objective**: Verify all voltage regulators produce output within specification.
**Test Steps**:

1. **SAFETY CHECKPOINT**: Before applying power, confirm
   - All connectors are properly seated
   - No visible solder bridges or defects
2. Turn **ON** 12V power supply output.  Immediately check and observe for:
   - Unusual noises or smells
   - Evidence of arcing or overheating
   - Discoloration or odor
3. Place multimeter ground probe on J8 Pin 14 (GND reference)
4. Measure voltage on +12V rail:
   - Verify +12V LED indication is illuminated (D17)
   - Place test probe on J8 pin 15
   - Expected reading: **12.0V**, actual: _______ V
5. Measure voltage on +5V rail:
   - Verify +5V LED indication is illuminated (D16)
   - Place test probe on J6 pin 4
   - Expected reading: **5.0V**, actual: _______ V
6. Measure voltage on +3.3V rail:
   - Verify +3.3V LED indication is illuminated (D15)
   - Place test probe on via near C6 pin 1 (3.3V output test point)
   - Expected reading: **3.3V**, actual: _______ V
7. If voltages are outside tolerance, STOP testing and investigate voltage regulator circuits

**Pass Criteria**: All three voltage measurements within specified tolerances

---

### 5.2 Sensor Input Test

**Objective**: Verify sensor input signal conditioning circuits (Schmitt triggers) are functional.

**Test Lane 1 Sensor Input**:

1. Place multimeter ground probe on J8 Pin 14 (GND reference)
2. Place multimeter test probe on **J8 Pin 5** (Lane 1 sensor input signal)
3. Inactive state (no stimulus):
   - Expected: **3.3V (HIGH)**, Actual: _______ V
   - Note: The sensor is active low but the circuit inverts the signal resulting in active high
4. Activate sensor: Pass car or dark object in under lane 1 finish-line sensor
   - Expected: **0V (LOW)**, Actual: _______ V
5. Deactivate sensor: Remove optical stimulus
   - Expected: **3.3V (HIGH)**, Actual: _______ V
6. Repeat activation/deactivation cycle 3 times to confirm signal responsiveness

**Test Lane 2 Sensor Input**:

1. Place multimeter ground probe on J8 Pin 14 (GND reference)
2. Place multimeter test probe on **J8 Pin 4** (Lane 2 sensor input signal)
3. Inactive state (no stimulus):
   - Expected: **3.3V (HIGH)**, Actual: _______ V
   - Note: The sensor is active low but the circuit inverts the signal resulting in active high
4. Activate sensor: Pass car or dark object in under lane 2 finish-line sensor
   - Expected: **0V (LOW)**, Actual: _______ V
5. Deactivate sensor: Remove optical stimulus
   - Expected: **3.3V (HIGH)**, Actual: _______ V
6. Repeat activation/deactivation cycle 3 times to confirm signal responsiveness

**Pass Criteria**:

- All voltage readings 0V or 3.3V (no intermediate values)
- Signal responds quickly to stimulus (no observable delay)
- No false triggers or chattering observed

---

### 5.3 Display Continuity and Power Test

**Objective**: Verify display driver outputs can illuminate LED segments; validate digit selection and decimal point functionality.
**Test Steps**:

#### Phase 1: Decimal Point (Ones Position)

Decimal point is only active on Ones digit (per design); simplest verification of display power & drivers

1. Confirm J6 pin 3 (GND) and J6 pin 4 (+5V) are connected to display power
2. Using jumper wire, connect J4 pin 5 (Decimal Point) to GND reference (J6 pin 3)
3. Visually inspect display: **Decimal point in Ones digit position should illuminate**
4. Release jumper; point should extinguish
5. Repeat jumper activation 3 times; confirm consistent illumination

#### Phase 2: Tens Digit Verify

MCD14543 digit demultiplexing and segment drivers are functional

1. Confirm J6 pin 3 (GND) and J6 pin 4 (+5V) are connected to display power
2. Using jumper wire, connect J4 pin 6 (Tens) to GND reference (J6 pin 3)
3. Assuming AD0-AD3 are low, the tens digit should illuminate "0" pattern (all segments except decimal point)
4. [Optional] Using jumper wire, connect J4 pin 2 (AD3) to J6 pin 4 (+5V), the tens digit should illuminate "1" pattern

**Pass Criteria**:

- Decimal point illuminates cleanly with 0V jumper, no dimming
- Decimal point extinguishes completely when jumper removed
- No other segments illuminate during decimal-only test
- No visible arcing or component overheating

**Notes**:

- Decimal point test is simplest & catches power/grounding issues
- Full 7-segment + multiplexing test deferred to firmware integration phase
- If decimal fails: suspect 5V supply, GND isolation, or MCD14543 output pin damage

---

## 6.  Teardown

1. Turn OFF the external 12V power supply
2. Remove all test equipment and jumper wires
3. Disconnect external peripherals (optical sensors, display connectors)
4. Visually inspect board for any damage or discoloration
5. Document any anomalies for rework

---

## 7. Test Results Summary

| Test Section | Status | Comments | Technician | Date |
| --- | --- | --- | --- | --- |
| 5.1 Power Supply | ☐ Pass ☐ Fail | | | |
| 5.2 Sensor Inputs | ☐ Pass ☐ Fail | | | |
| 5.3 Display Continuity | ☐ Pass ☐ Fail | | | |

**Overall Status**: ☐ PASS ☐ CONDITIONAL ☐ FAIL
**Notes / Action Items**:

---

---

---

---

## Appendix A: Connector Pinout Quick Reference

### External Connectors

| Connector | Purpose | Pin 1 | Pin 2 | Pin 3 | Pin 4 | Pin 5 | Pin 6 | Pin 7 | Pin 8 | Pin 9 | Pin 10 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **J1** | Sensor Input | +12V | Lane 1 | Lane 2 | GND | | | | | | |
| **J2** | Serial | RXD | TXD | GND | | | | | | | |
| **J3** | Power | +12V | GND | | | | | | | | |
| **J4** | Right Display | AD0 | AD3 | AD1 | AD2 | Decimal | Tens | Ones | Tenths | Hundredths | Thousandths |
| **J5** | Left Display | AD0 | AD3 | AD1 | AD2 | Decimal | Tens | Ones | Tenths | Hundredths | Thousandths |
| **J6** | Display Power | +12V | GND | GND | +5V | | | | | | |

### J8 Arduino Header Connections (Analog)

| Pin | Purpose | Arduino Pin | Notes |
| --- | --- | --- | --- |
| 1 | N/C | D13/SCK | Not used |
| 2 | N/C | +3.3V | Arduino-generated 3.3V reference (not for power) |
| 3 | N/C | AREF | Arduino analog reference input |
| 4 | Lane 2 | A0 | Lane 2 sensor input |
| 5 | Lane 1 | A1 | Lane 1 sensor input |
| 6 | BCD_Lane1 | A2 | Select (enable) for lane 1 74HCD238 |
| 7 | BCD_Lane2 | A3 | Select (enable) for lane 2 74HCD238 |
| 8 | N/C | A4/SDA | Not used |
| 9 | N/C | A5/SCL | Not used |
| 10 | N/C | A6 | Not used |
| 11 | N/C | A7 | Not used |
| 12 | N/C | +5VDC | Arduino-generated 5V reference (not for power) |
| 13 | N/C | ~Reset | Reset low |
| 14 | GND | GND | Power Ground |
| 15 | +12V | +12V | Power Input |

### J9 Arduino Header Connections (Digital)

| Pin | Purpose | Arduino Pin | Notes |
| --- | --- | --- | --- |
| 1 | N/C | D12/MISO | RFID MISO |
| 2 | N/C | D11/MOSI | RFID MOSI |
| 3 | N/C | D10/SS | RFID Control Select Right |
| 4 | Decimal Point | D9 | Pass through 74CHT244 buffer |
| 5 | AD3 | D8 | Pass through 74CHT244 buffer |
| 6 | AD2 | D7 | Pass through 74CHT244 buffer |
| 7 | AD1 | D6 | Pass through 74CHT244 buffer |
| 8 | AD0 | D5 | Pass through 74CHT244 buffer |
| 9 | BCD_MuxC | D4 | Pass through 74CHT244 buffer |
| 10 | BCD_MuxB | D3 | Pass through 74CHT244 buffer |
| 11 | BCD_MuxA | D2 | Pass through 74CHT244 buffer |
| 12 | GND | GND | Power Ground |
| 13 | N/C | ~Reset | Reset low |
| 14 | RDX | D0/Rx | Serial receive (Start Controller) |
| 15 | TXD | D1/Tx | Serial transmit (Start Controller) |

---

## Appendix B: Troubleshooting Guide

1. Sensors not triggering (both lanes stuck HIGH)
   - Confirm +3.3V supply to logic gate (74LVC2G14)
   - Physically inspect lens for dust/blockage
   - Check +12V supply to sensor connector (J1 pin 1)
   - Pattern: If stuck HIGH → sensor output or logic gate failed; inspect with continuity test
2. One lane sensor fails, other works
   - Verify sensor +12V and GND continuity from J1 to physical connector
   - Check 74LVC2G14 isolation diodes for leakage (usually in parallel per input stage)
   - Pattern: Suggests single-sensor failure or isolation component degradation
3. Inconsistent sensor triggering (chattering, missed cars)
   - Check QS-18 lens alignment (sensor should be square to beam path)
   - Verify cable shielding integrity (especially on longer cable runs >3m)
   - Slow trigger response → check Schmitt trigger capacitor values (typically 100nF across logic IC)
