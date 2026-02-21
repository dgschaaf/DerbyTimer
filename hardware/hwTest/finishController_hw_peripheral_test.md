# Finish Controller Hardware Peripheral Test

## 1. Objective

The purpose of this test is to validate basic peripheral connectivity and power supply integrity of the Finish Controller PCB before software/firmware integration. This test checks that all external connectors are properly wired, all voltage regulators are functioning within specification, and sensor input signal conditioning circuits are present and functional.

**Scope**:

- Fully populated and assembled PCB
- All external peripherals properly connected
- External 12VDC power supply connected
- **NOT included**: Functional testing with active firmware control (deferred to integration testing phase)

**Success Criteria**: All measured voltages within specification, all external signals respond to hardware stimulation as expected.

---

## 2. Background

The Finish Controller board is a specialized parallel-port interface designed to:

- **Accept optical sensor inputs** from two independent finish-line detection lanes (Lane 1 and Lane 2)
- **Condition and amplify sensor signals** using Schmitt-triggered logic gates (74LVC2G14) for signal integrity
- **Provide time-display output signals** that drive two independent 7-segment LED digit displays with multiplexed selection
- **Manage multiple voltage domains** including raw 12V input, regulated 5V, and regulated 3.3V supplies
- **Interface with the Race Manager controller** via logic-level-shifted RS-232 serial communication

This peripheral test validates that the signal paths, power distribution, and passive circuit elements are correctly assembled and functional. Active firmware testing of display multiplexing and timing functions is deferred to the integration testing phase.

---

## 3. Equipment Required

- Digital multimeter with DC voltage measurement capability (resolution to 0.1V)
- External 12VDC regulated power supply (minimum 2A capacity)
- Pair of optical sensors (same type as finish-line sensors) for stimulation testing
- Non-contact IR thermometer (optional, for thermal monitoring)
- Jumper wires (22 AWG) for temporary connections
- ESD protection mat and wrist strap
- Schematic printouts (optional, for reference during testing)

---

## 4. Connector Pinout Reference

| Connector | Pins | Purpose | Pin Description |
| ----------- | ------ | --------- | ----------------- |
| **J3** | 2 | 12V Power Input | Pin 1: +12V Source, Pin 2: GND |
| **J1** | 4 | Sensor Input | Pin 1: +12V (sensor supply), Pin 2: Lane 1 input, Pin 3: Lane 2 input, Pin 4: GND |
| **J6** | 4 | Display Power | Pin 1: +12V, Pin 2: GND, Pin 3: GND, Pin 4: +5V |
| **J5** | 10 | Time 1 Display Output (Lane 1) | Segment data lines for left 7-digit display |
| **J4** | 10 | Time 2 Display Output (Lane 2) | Segment data lines for right 7-digit display |
| **J2** | 3 | Serial Communications | TXD, RXD, GND to Race Manager controller |
| **J8** | 15 | Microprocessor Interface (Reserved) | Future firmware control interface |
| **J9** | 15 | Microprocessor Interface (Reserved) | Future firmware control interface |

---

## 5. Pre-Test Verification and Setup

### 5.1 Pre-Test Visual Inspection

Before applying power to the circuit, perform the following checks:

1. **Solder joint inspection**:
   - Visually inspect all solder joints under magnification (10x recommended)
   - Verify proper wetting (shiny appearance) with no bridges or cold joints
   - Check for component rotation, lifting, or popcorning
2. **Component verification**:
   - Confirm all decoupling capacitors are installed on IC power pins
   - Check voltage regulator presence (AP2112K-3.3 for 3.3V, LDO for 5V)
   - Verify no reversed polarity diodes or capacitors
   - Ensure all IC packages are properly seated and not lifted
3. **Connector inspection**:
   - All connectors properly soldered with clean, complete connections
   - No cold solder joints or connector misalignment
   - Connector keying matches intended peripheral (if applicable)
4. **Schematic compliance**:
   - Board revision code matches test documentation
   - All jumper and configuration settings are set per design intent
   - No DNP (Do Not Populate) components in critical power or signal paths

### 5.2 Power Supply Configuration

1. Set external 12V power supply to exactly **12.0V ± 0.5V** before connecting to board
2. Enable current limiting feature on supply (if available) set to approximately **2.0A**
3. Leave power supply **OFF** until directed in test procedure
4. Verify power supply ground lead connection is secure

### 5.3 Peripheral Connection

1. **Power Supply**: Connect external 12V power supply to connector **J3** (2-pin power input)
   - Pin 1: +12V
   - Pin 2: GND1.
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

## 6. Test Procedure

### 6.1 Power Supply Voltage Verification

**Objective**: Verify all voltage regulators produce stable output within specification.

**Safety Warning**: Before energizing the board, perform final visual inspection for:

- Solder bridges across power traces
- Reversed polarity components
- Missing or misaligned integrated circuits
- Any trace damage or burn marks

**Test Steps**:

1. Connect external 12V power supply leads to J3 (Pin 1: +12V, Pin 2: GND)
2. Set multimeter to **DC Voltage mode, 20V range**
3. Connect multimeter ground probe to J3 Pin 2 (GND reference)
4. **SAFETY CHECKPOINT**: Before applying power, confirm:
   - All connectors are properly seated
   - No visible solder bridges or defects
   - Visual inspection complete and documented
5. Turn ON external 12V power supply
6. **IMMEDIATE OBSERVATION**: Listen and observe for:
   - Unusual noises from board
   - Evidence of arcing or overheating
   - Component discoloration or odors
   - If any anomaly detected, turn OFF power immediately
7. Wait 10 seconds for voltage regulators to stabilize and filtering capacitors to charge
8. **Measure +12V supply input voltage** at J3:
   - Place multimeter probe on J3 Pin 1
   - **Expected**: 12.0V ± 0.5V
   - **Record**: _______ V ☐ PASS ☐ FAIL
9. **Measure +5V regulated output voltage**:
   - Locate +5V supply test point on PCB (or measure at J6 Pin 4)
   - **Expected**: 5.0V ± 0.2V
   - **Record**: _______ V ☐ PASS ☐ FAIL
10. **Measure +3.3V regulated output voltage**:
   - Locate +3.3V supply test point on PCB (or at sensor circuit VCC)
   - **Expected**: 3.3V ± 0.1V
   - **Record**: _______ V ☐ PASS ☐ FAIL
11. **Verify voltage stability**: Wait 30 seconds and re-measure all three voltages
   - All readings should remain stable ± 0.1V from initial measurement
   - **Record**: 12V: _______ V, 5V: _______ V, 3.3V: _______ V
12. If any voltage is out of tolerance, turn OFF power supply and investigate voltage regulator circuits

**Pass Criteria**:

- ✓ All three voltages within specification
- ✓ No oscillation or drift > 0.1V over 30-second period
- ✓ No excessive component heating (hand-touch test)

---

### 6.2 Sensor Input Signal Path Test

**Objective**: Verify sensor input signal conditioning circuits (Schmitt triggers) are functional.

**Circuit Reference**:

- U3 (74LVC2G14) dual Schmitt trigger gates on FinishSensors.kicad_sch
- Pull-up resistors on Lane 1 and Lane 2 inputs
- 3.3V logic level output

**Test Lane 1 Sensor Input**:

1. Ensure 12V power supply is ON and voltages verified (Section 6.1)
2. Set multimeter to **DC Voltage mode, 5V range**
3. Place multimeter ground probe on J1 Pin 4 (GND reference)
4. Place multimeter test probe on **J1 Pin 2** (Lane 1 sensor input signal)
5. **Inactive state** (no stimulus):
   - Expected: **3.3V (HIGH)**
   - Record: _______ V
6. **Activate sensor**: Pass optical sensor or object in front of finish-line sensor
   - Expected: **0V (LOW)**
   - Record: _______ V
7. **Deactivate sensor**: Remove optical stimulus
   - Expected: **3.3V (HIGH)**
   - Record: _______ V
8. **Repeat activation/deactivation cycle** 3 times to confirm signal responsiveness
   - Activation cycle 1: HIGH → LOW → HIGH ☐ PASS ☐ FAIL
   - Activation cycle 2: HIGH → LOW → HIGH ☐ PASS ☐ FAIL
   - Activation cycle 3: HIGH → LOW → HIGH ☐ PASS ☐ FAIL

**Test Lane 2 Sensor Input**:

1. Place multimeter test probe on **J1 Pin 3** (Lane 2 sensor input signal)
2. **Inactive state** (no stimulus):
   - Expected: **3.3V (HIGH)**
   - Record: _______ V
3. **Activate sensor**: Pass optical sensor in front of finish-line sensor
   - Expected: **0V (LOW)**
   - Record: _______ V
4. **Deactivate sensor**: Remove optical stimulus
   - Expected: **3.3V (HIGH)**
   - Record: _______ V
5. **Repeat activation/deactivation cycle** 3 times
   - Activation cycle 1: HIGH → LOW → HIGH ☐ PASS ☐ FAIL
   - Activation cycle 2: HIGH → LOW → HIGH ☐ PASS ☐ FAIL
   - Activation cycle 3: HIGH → LOW → HIGH ☐ PASS ☐ FAIL

**Pass Criteria**:

- ✓ All voltage readings exactly 0V or 3.3V (no intermediate values)
- ✓ Signal responds quickly to stimulus (< 100ms)
- ✓ No false triggers or chattering observed

---

### 6.3 Display Power Supply Verification

**Objective**: Verify display power distribution circuit provides correct supply voltages.

**Test Steps**:

1. Ensure 12V power supply remains ON
2. Set multimeter to **DC Voltage mode, 20V range**
3. Place multimeter ground probe on **J6 Pin 2** (GND reference for display power)
4. **Measure +12V display supply**:
   - Place test probe on J6 Pin 1
   - Expected: **12.0V ± 0.5V**
   - Record: _______ V ☐ PASS ☐ FAIL
5. **Measure +5V display supply**:
   - Place test probe on J6 Pin 4
   - Expected: **5.0V ± 0.2V**
   - Record: _______ V ☐ PASS ☐ FAIL
6. **Verify ground continuity**: Measure resistance between J6 Pin 2 and J6 Pin 3
   - Set multimeter to **Resistance (Ω) mode**
   - Expected: **< 0.5Ω** (should measure nearly 0Ω)
   - Record: _______ Ω ☐ PASS ☐ FAIL

**Pass Criteria**:

- ✓ +12V supply within tolerance
- ✓ +5V supply within tolerance
- ✓ Ground pins securely connected (low resistance)

---

### 6.4 Display Data Output Lines Continuity Check

**Objective**: Verify display segment data output lines (J4/J5) have continuity and are not shorted together.

**Test Steps**:

1. Turn OFF the 12V power supply (safety precaution for continuity testing)
2. Allow board to discharge for 10 seconds
3. Set multimeter to **Resistance (Ω) mode, 200Ω range**
4. **Check Lane 1 display outputs (J5)**:
   - Measure resistance between J5 Pin 1 and J5 Pin 2
   - Expected: **> 10kΩ** (open circuit, no short)
   - Record: _______ Ω ☐ PASS ☐ FAIL
   - Test at least 3 different pin-pair combinations to detect segment short circuits
5. **Check Lane 2 display outputs (J4)**:
   - Measure resistance between J4 Pin 1 and J4 Pin 2
   - Expected: **> 10kΩ**
   - Record: _______ Ω ☐ PASS ☐ FAIL
   - Test at least 3 different pin-pair combinations

**Pass Criteria**:

- ✓ All pin-pair resistances > 10kΩ (no shorts between output lines)
- ✓ No unusually low resistance readings indicating solder bridges

---

### 6.5 Serial Communication Interface Continuity

**Objective**: Verify RS-232 level shifter connections and serial interface lines have proper continuity.

**Circuit Reference**: LogicLevelShifter_sch.kicad_sch (converts 3.3V logic → ±12V RS-232)

**Test Steps**:

1. Power supply remains OFF
2. Set multimeter to **Resistance (Ω) mode**
3. Measure resistance between **J2 Pin 1** (TXD) and **J2 Pin 3** (GND):
   - Expected: **Low resistance path** (< 1kΩ if connected to active circuit)
   - Record: _______ Ω
4. Measure resistance between **J2 Pin 2** (RXD) and **J2 Pin 3** (GND):
   - Expected: **Low resistance path** (< 1kΩ if connected to active circuit)
   - Record: _______ Ω
5. Verify **no direct shorts** between TXD and RXD:
   - Measure resistance between J2 Pin 1 and J2 Pin 2
   - Expected: **> 1kΩ** (no short)
   - Record: _______ Ω ☐ PASS ☐ FAIL

**Pass Criteria**:

- ✓ No shorts between communication signal lines
- ✓ Ground connections properly established

---

## 7. Test Teardown

1. Turn OFF the external 12V power supply
2. Allow the board to discharge for 5 seconds
3. Disconnect all test equipment leads in reverse connection order:
   - Remove multimeter probes
   - Disconnect power supply leads from J3
   - Remove any temporary jumper wires
4. Remove external peripherals (optical sensors, display connectors) if applicable
5. Perform final visual inspection:
   - Check for any component discoloration or burn marks
   - Verify no solder splashes or cold joints introduced during testing
   - Document any anomalies observed
6. Store board in ESD-safe packaging

---

## 8. Test Results Summary

| Test Section | Status | Pass/Fail | Notes | Technician | Date |
| --- | --- | --- | --- | --- | --- |
| 6.1 Power Supply Voltages | ☐ Complete | ☐ P ☐ F | 12V: ___V, 5V:___V, 3.3V: ___V | | |
| 6.2 Sensor Lane 1 Response | ☐ Complete | ☐ P ☐ F | All 3 cycles passed | | |
| 6.2 Sensor Lane 2 Response | ☐ Complete | ☐ P ☐ F | All 3 cycles passed | | |
| 6.3 Display Power Supplies | ☐ Complete | ☐ P ☐ F | 12V: ___V, 5V:___V | | |
| 6.4 Display Continuity | ☐ Complete | ☐ P ☐ F | No shorts detected | | |
| 6.5 Serial Interface | ☐ Complete | ☐ P ☐ F | No line shorts | | |

**Overall Status**: ☐ PASS ☐ CONDITIONAL ☐ FAIL

**Anomalies/Rework Required**:

---

---

**Test Conducted**: Date _________ Time _________

**Technician Signature**: _________________________

**QA Review Signature**: _________________________

---

## 9. Review Summary and Recommendations

### 9.1 Protocol Strengths

✓ **Comprehensive power domain testing**: All three regulated supplies (12V, 5V, 3.3V) are verified  
✓ **Signal-level validation**: Sensor input circuits tested with actual stimulation (not just continuity)  
✓ **Proper sequencing**: Safety checks precede power-on operation  
✓ **Clear pass/fail criteria**: All measurements reference specific tolerances  
✓ **Equipment-level detail**: Test equipment settings specified (range, probe locations)  

### 9.2 Potential Improvements for Future Rev

1. **Add connector diagrams**: Include physical connector outlines showing pin locations for reference
2. **Current draw monitoring**: Add optional current consumption measurements:
   - Quiescent current draw at +12V: expect < 50mA (logic only)
   - Peak current during sensor activity: expect < 100mA
3. **Thermal monitoring**: Add optional IR thermometer readings:
   - Maximum acceptable regulator temperature: < 60°C
   - Note any hot spots or asymmetric heating
4. **Signal timing test**: For future firmware integration:
   - Sensor pulse width and hysteresis measurement
   - Output multiplexer switching timing verification (74HCT238)
5. **Documentation references**: Add hyperlinks to schematic pages in production schematics
   - Section 6.2: Reference FinishSensors.kicad_sch page for circuit details
   - Section 6.5: Reference LogicLevelShifter_sch.kicad_sch for RS-232 topology
6. **Failure mode table**: Add troubleshooting guide for common failures:
   - Low or zero voltage readings → suspect regulator IC or input filtering
   - No sensor response → suspect Schmitt trigger IC (U3) or pull-up resistors
   - Display shorts → suspect segment data line solder bridges

### 9.3 Compliance with Hobbyist Project Philosophy

This protocol balances **thoroughness** with **practical implementation**:

- ✓ Tests critical functions (power, signals) without excessive instrumentation requirements
- ✓ Uses basic tools (multimeter, optical sensors) that are commonly available
- ✓ Estimated test time: 30-45 minutes for experienced technician
- ✓ Provides clear objective criteria (not subjective "looks good" assessment)
- ✓ Documents results for repeatability and quality tracking
- ✓ Clearly defers complex functional testing to firmware integration phase

### 9.4 Next Steps

After this peripheral test passes:

1. **Firmware Integration Test** (future document):
   - Load microcontroller with timing software
   - Verify 74HCT238 demultiplexer selection logic
   - Test 7-segment display pattern generation
   - Validate serial communication with Race Manager

2. **System Integration Test** (final phase):
   - Integrate with optical gate sensors
   - Integrate with display modules
   - End-to-end timing accuracy validation
   - Safety and operational acceptance testing

---

## Appendix A: Connector Pinout Quick Reference

| Connector | Purpose | Pin 1 | Pin 2 | Pin 3 | Pin 4 |
| --- | --- | --- | --- | --- | --- |
| **J3** | Power Input | +12V | GND | — | — |
| **J1** | Sensor Input | +12V Supply | Lane 1 Data | Lane 2 Data | GND |
| **J6** | Display Power | +12V | GND | GND | +5V |
| **J5** | Lane 1 Display | Seg Data Lines (10 pins) | | | |
| **J4** | Lane 2 Display | Seg Data Lines (10 pins) | | | |
| **J2** | Serial | TXD | RXD | GND | — |

---

## Appendix B: Voltage Measurement Checklist

- [ ] 12V input: 12.0V ± 0.5V (stable for 30+ seconds)
- [ ] 5V output: 5.0V ± 0.2V (stable for 30+ seconds)
- [ ] 3.3V output: 3.3V ± 0.1V (stable for 30+ seconds)
- [ ] Lane 1 sensor: toggles cleanly 0V ↔ 3.3V when stimulated
- [ ] Lane 2 sensor: toggles cleanly 0V ↔ 3.3V when stimulated
- [ ] Display J5 outputs: > 10kΩ between any two pins (no shorts)
- [ ] Display J4 outputs: > 10kΩ between any two pins (no shorts)
- [ ] Serial interface J2: no shorts between TXD/RXD lines

---

## Appendix C: Troubleshooting Guide

**No +12V output?**
- Check main input connector J3 polarity
- Verify input fuse protection
- Check power supply ground connection

**Low 5V or 3.3V voltage?**
- Verify regulator ICs are soldered and present
- Check input supply is present
- Measure for excessive current draw (short circuit)

**Sensor inputs not responding?**
- Check U3 (74LVC2G14) Schmitt trigger IC for power and ground
- Verify pull-up resistor connections
- Test with multimeter: expected 3.3V HIGH, 0V LOW when stimulated

**Display outputs shorted?**
- Check for solder bridges between segment data lines
- Verify 74HCT244 buffer IC connections
- Re-flow any problematic pad connections
