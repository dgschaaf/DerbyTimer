# Finish Controller Hardware Peripheral Test

## 1. Objective

The objective of this test is to perform basic checkout of the Finish Controller PCB before software/firmware integration. This test checks all external connectors, all voltage regulators, and sensor input signal conditioning circuits.

---

## 2. Background

The Finish Controller board is part of hte derby timer system and is designed to be a custom Arduino shield at the finish end of hte race track.  Its major functions are:

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

1. **SAFETY CHECKPOINT**: Before applyihg power, confirm
   - All connectors are properly seated
   - No visible solder bridges or defects
2. Turn **ON** 12V power supply output.  Immeadiately check and observe for:
   - Unusual noises or smells
   - Evidence of arcing or overheating
   - Dicoloroation or odor
3. Place multimeter ground probe on J3 Pin 2 (GND reference)
4. Measure voltage on +12V rail:
   - Verify +12V LED indication is illuminated (D17)
   - Place test probe on J3 pin 1
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

1. Place multimeter ground probe on J1 Pin 4 (GND reference)
2. Place multimeter test probe on **J1 Pin 2** (Lane 1 sensor input signal)
3. Inactive state (no stimulus):
   - Expected: **3.3V (HIGH)**, Actual: _______ V
4. Activate sensor: Pass optical sensor or object in front of finish-line sensor
   - Expected: **0V (LOW)**, Actual: _______ V
5. Deactivate sensor: Remove optical stimulus
   - Expected: **3.3V (HIGH)**, Actual: _______ V
6. Repeat activation/deactivation cycle 3 times to confirm signal responsiveness

**Test Lane 2 Sensor Input**:

1. Place multimeter ground probe on J1 Pin 4 (GND reference)
2. Place multimeter test probe on **J1 Pin 3** (Lane 2 sensor input signal)
3. Inactive state (no stimulus):
   - Expected: **3.3V (HIGH)**, Actual: _______ V
4. Activate sensor: Pass optical sensor in front of finish-line sensor
   - Expected: **0V (LOW)**, Actual: _______ V
5. Deactivate sensor: Remove optical stimulus
   - Expected: **3.3V (HIGH)**, Actual: _______ V
6. Repeat activation/deactivation cycle 3 times to confirm signal responsiveness

**Pass Criteria**:

- All voltage readings 0V or 3.3V (no intermediate values)
- Signal responds quickly to stimulus (< 100ms)
- No false triggers or chattering observed

---

### 5.3 Display Continuity and Power Test

**Objective**: Deferred

**Test Steps**:

1. Deferred

**Pass Criteria**:

- Deferred

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

| Connector | Purpose | Pin 1 | Pin 2 | Pin 3 | Pin 4 | Pin 5 | Pin 6 | Pin 7 | Pin 8 | Pin 9 | Pin 10 |
| --- | --- | --- | --- | --- | --- |
| **J1** | Sensor Input | +12V | Lane 1 | Lane 2 | GND | | | | | | |
| **J2** | Serial | RXD | TXD | GND | | | | | | | | |
| **J3** | Power | +12V | GND | | | | | | | | | |
| **J4** | Right Display | AD0 | AD3 | AD1 | AD2 | Decimal | Tens | Ones | Tenths | Hundredths | Thousandths |
| **J5** | Left Display | AD0 | AD3 | AD1 | AD2 | Decimal | Tens | Ones | Tenths | Hundredths | Thousandths |
| **J6** | Display Power | +12V | GND | GND | +5V | | | | | | | |

---

## Appendix B: Troubleshooting Guide

TBD
