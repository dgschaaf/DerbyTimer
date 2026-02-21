# Finish Controller Hardware Functional Test

## 1. Objective

The purpose of this test is to validate that the Finish Controller PCB firmware executes correctly on the Arduino microprocessor and that all connected peripherals (optical sensors, 7-segment displays) respond to software commands as expected. This test verifies active firmware control of the circuit, following successful completion of the peripheral hardware test (static continuity and voltage verification).

**Scope**:

- Arduino Nano 33 BLE properly installed on PCB
- All peripheral connections verified (sensors, displays)
- Firmware loaded and Serial Monitor communication established
- **Includes**: Functional testing with active firmware control of sensors and displays
- **NOT included**: Timing accuracy or race-condition functionality (deferred to system integration testing)

**Success Criteria**: All sensors report correct state changes, display segments illuminate correctly under software control, serial communication functions properly.

---

## 2. Background

The Finish Controller PCB provides a measurement and display interface for a derby timer system. The firmware running on the Arduino Nano 33 BLE microprocessor controls:

- **Sensor Signal Polling**: Reads optical sensor inputs (Lane 1 and Lane 2) previously conditioned by Schmitt trigger circuits (U3: 74LVC2G14)
- **Display Multiplexing**: Controls digit selection via a 3-to-8 decoder (U4: 74HCT238) to individually address each of five 7-segment displays per lane
- **Segment Data Output**: Drives 7-segment display data lines through output buffers (74HCT244) to display timing values
- **Serial Communication**: Sends timing results and test data to the Race Manager controller via logic-level-shifted RS-232 interface
- **PWM Control**: Optional PWM-controlled LED brightness (if implemented)

This functional test validates that the firmware correctly reads sensor states, controls the display multiplexing logic, and maintains proper serial communication—verifying that hardware and firmware integrate correctly.

---

## 3. Equipment Required

- Laptop running Arduino IDE (v1.8.13 or later)
- USB 2.0 cable (USB Type-A to Micro-B for Arduino Nano)
- Arduino Nano 33 BLE (nRF52840 microprocessor)
- Pair of optical sensors (same type as actual finish-line sensors) for stimulus testing
- 12V regulated power supply (from peripheral test) with current limiting enabled
- Serial terminal monitor (Arduino IDE Serial Monitor or equivalent)
- Optional: Small object (card, cube) for blocking optical sensors during testing

---

## 4. Pre-Test Setup and Safety

### 4.1 Hardware Verification

1. **Pressure test completion**: Confirm that the Finish Controller peripheral hardware test (Section 6 of finishController_hw_peripheral_test.md) has been successfully completed and documented
2. **Component installation verification**:
   - Arduino Nano 33 BLE is fully seated in the microprocessor headers (no lifted pins or rotation)
   - All display and sensor connectors are securely connected
   - No visible damage, bent leads, or solder bridges
3. **Power supply safety**:
   - External 12V power supply set to 12.0V ± 0.3V
   - Current limiting set to 1.5A minimum
   - Power supply remains **OFF** until directed in test steps

### 4.2 Firmware Loading

1. Open Arduino IDE on laptop
2. Open File → Examples → or load finishController_hw_functional_test.ino
3. Select Tools → Board: "Arduino Nano 33 BLE"
4. Select Tools → Port: "COM[X]" (appropriate USB serial port)
5. Verify baud rate is set to **9600 baud**
6. Click Upload button; wait for "Done uploading" message
7. Do **not** open Serial Monitor yet—wait for test procedure

---

## 5. Test Procedure

### 5.1 Serial Monitor Communication Verification

**Objective**: Verify that the Arduino Nano boots correctly and establishes Serial Monitor communication.

**Test Steps**:

1. **Apply 12V power**:
   - Turn ON the external 12V power supply connected to J3
   - Wait 2 seconds for voltage regulators to stabilize
2. **Connect to USB**: Connect USB cable from laptop to Arduino Nano Micro-B connector
3. **Open Serial Monitor**:
   - Arduino IDE → Tools → Serial Monitor
   - Set baud rate to **9600 baud**
   - Set line ending to "Newline"
4. **Wait for startup message**:
   - Expected output: `Finish Controller Functional Test Starting...` or similar startup message
   - If no message appears: Check USB port selection and reload firmware
   - Record: ☐ Startup message received **PASS** ☐ FAIL
5. **Command prompt ready**:
   - Expected output: `Ready for commands. Enter test command:` or similar prompt
   - Record: ☐ Command prompt visible **PASS** ☐ FAIL

**Pass Criteria**:

- ✓ Serial communication established at 9600 baud
- ✓ Startup message received within 5 seconds
- ✓ Command prompt displayed and ready for input

---

### 5.2 Sensor Input Test - Lane 1

**Objective**: Verify that firmware correctly reads Lane 1 optical sensor input and reports state changes.

**Circuit Reference**: FinishSensors.kicad_sch (U3: 74LVC2G14 Schmitt trigger conditioning)

**Test Steps**:

1. Ensure Serial Monitor is open with 9600 baud connection
2. **Activate continuous sensor monitoring**:
   - Type: `sensor_test` or `read_sensors` (command may vary based on firmware)
   - Press Enter
   - Expected output: Firmware begins polling sensors and printing current state (e.g., `Lane 1: HIGH Lane 2: HIGH`)
3. **Test Lane 1 inactive state**:
   - Ensure no object is blocking Lane 1 optical sensor
   - Expected output: `Lane 1: HIGH` (sensor line pulled high by pull-up resistor)
   - Record output: _________________ - **PASS** ☐ FAIL
4. **Test Lane 1 active state**:
   - Block Lane 1 sensor by passing object or sensor simulator in front of finish-line sensor
   - Expected output: `Lane 1: LOW` (sensor line pulled low by optical sensor)
   - Record activation response time: < 100ms - **PASS** ☐ FAIL
5. **Test sensor responsiveness**:
   - Remove blocking object
   - Expected output: `Lane 1: HIGH` (returns to inactive state)
   - Record deactivation response time: < 100ms - **PASS** ☐ FAIL
6. **Repeat activation cycles**:
   - Perform block/unblock cycle 3 additional times
   - Activation Cycle 1: HIGH → LOW → HIGH ☐ **PASS** ☐ FAIL
   - Activation Cycle 2: HIGH → LOW → HIGH ☐ **PASS** ☐ FAIL
   - Activation Cycle 3: HIGH → LOW → HIGH ☐ **PASS** ☐ FAIL

**Pass Criteria**:

- ✓ Inactive state reads HIGH (3.3V logic level from Schmitt trigger output)
- ✓ Active state reads LOW (0V logic level when sensor triggered)
- ✓ All state transitions occur within < 100ms
- ✓ No chattering, false triggers, or unstable readings

---

### 5.3 Sensor Input Test - Lane 2

**Objective**: Verify that firmware correctly reads Lane 2 optical sensor input.

**Test Steps**:

1. **Continue sensor monitoring** (from Section 5.2):
   - If monitoring has stopped, type: `sensor_test` again
2. **Test Lane 2 inactive state**:
   - Ensure no object blocks Lane 2 optical sensor
   - Expected output: `Lane 2: HIGH`
   - Record output: _________________ - **PASS** ☐ FAIL
3. **Test Lane 2 active state**:
   - Block Lane 2 sensor with object or sensor stimulus
   - Expected output: `Lane 2: LOW`
   - Record response time: < 100ms - **PASS** ☐ FAIL
4. **Test Lane 2 responsiveness**:
   - Remove blocking object
   - Expected output: `Lane 2: HIGH`
   - Record response time: < 100ms - **PASS** ☐ FAIL
5. **Repeat activation cycles**:
   - Perform block/unblock cycle 3 additional times
   - Activation Cycle 1: HIGH → LOW → HIGH ☐ **PASS** ☐ FAIL
   - Activation Cycle 2: HIGH → LOW → HIGH ☐ **PASS** ☐ FAIL
   - Activation Cycle 3: HIGH → LOW → HIGH ☐ **PASS** ☐ FAIL

**Pass Criteria**:

- ✓ Inactive state reads HIGH
- ✓ Active state reads LOW
- ✓ All state transitions occur within < 100ms
- ✓ Consistent behavior across all activation cycles

---

### 5.4 Simultaneous Lane Sensor Test

**Objective**: Verify that firmware correctly tracks both sensors simultaneously without crosstalk or state interference.

**Test Steps**:

1. **Activate dual-sensor monitoring**:
   - Type: `sensor_test` (both lanes monitored simultaneously)
   - Expected: Both Lane 1 and Lane 2 states displayed continuously
2. **Test Lane 1 alone**:
   - Block Lane 1 sensor
   - Expected output: `Lane 1: LOW Lane 2: HIGH`
   - Record: ☐ **PASS** ☐ FAIL
3. **Test Lane 2 alone**:
   - Unblock Lane 1, then block Lane 2 sensor
   - Expected output: `Lane 1: HIGH Lane 2: LOW`
   - Record: ☐ **PASS** ☐ FAIL
4. **Test both lanes simultaneously**:
   - Block both Lane 1 and Lane 2 sensors
   - Expected output: `Lane 1: LOW Lane 2: LOW`
   - Record: ☐ **PASS** ☐ FAIL
5. **Test sequential activation**:
   - Unblock both lanes
   - Block Lane 1, then Lane 2 (without releasing Lane 1 first)
   - Expected output: `Lane 1: LOW Lane 2: LOW` → `Lane 1: LOW Lane 2: HIGH` (on Lane 1 unblock) → `Lane 1: HIGH Lane 2: LOW` (on Lane 2 unblock)
   - Record sequence: ☐ **PASS** ☐ FAIL

**Pass Criteria**:

- ✓ Both lanes report correct states simultaneously
- ✓ No cross-channel interference when one lane triggers
- ✓ Sequential activation follows expected state transitions
- ✓ State transitions are independent between lanes

---

### 5.5 Display Output Control Test - Lane 1 Display

**Objective**: Verify firmware correctly controls the 7-segment display multiplexing and segment data output for Lane 1.

**Circuit Reference**: Digit_Demux.kicad_sch (U4: 74HCT238 3-to-8 decoder for digit selection)

**Test Steps**:

1. **Activate Lane 1 display test**:
   - Type: `display1_test` or `left_display_test` (command varies by firmware)
   - Press Enter
   - Expected behavior: Firmware systematically cycles through all 7-segment patterns to test each digit and segment
2. **Visual inspection of Lane 1 display**:
   - Observe all five 7-segment digits in the left display module
   - Expected: Segments illuminate in recognizable patterns (numbers 0-9 or all segments)
   - Method: Firmware may cycle through `0 1 2 3 4 5 6 7 8 9` or display all segments lit, then all off
   - Record observed pattern: _________________
   - **PASS**: All 5 digits respond and display clearly ☐ FAIL: Segments missing or not illuminating
3. **Test digit multiplexing**:
   - Expected: Each digit brightens/dims as the multiplexer selects it
   - Expected timing: Each digit selected ~5-10ms per cycle (smooth, no flicker)
   - Observation: ☐ All digits multiplexing correctly **PASS** ☐ Some digits do not light **FAIL**
4. **Test decimal point (if implemented)**:
   - Expected: Decimal point LED cycles on/off with test pattern
   - Observation: ☐ Decimal point cycles correctly **PASS** ☐ Not illuminating **FAIL**
5. **Record test result**:
   - All 5 digits active and displaying: ☐ **PASS** ☐ FAIL
   - Proper multiplexing timing (no visible flicker): ☐ **PASS** ☐ FAIL
   - Decimal point functional: ☐ **PASS** ☐ FAIL

**Pass Criteria**:

- ✓ All 5 digit positions illuminate under firmware control
- ✓ No segments remain permanently on or off (dead segments)
- ✓ Decimal point (if present) responds to firmware commands
- ✓ Multiplexing frequency sufficient to avoid visible flicker (> 30Hz refresh rate)

---

### 5.6 Display Output Control Test - Lane 2 Display

**Objective**: Verify firmware correctly controls the 7-segment display for Lane 2.

**Test Steps**:

1. **Activate Lane 2 display test**:
   - Type: `display2_test` or `right_display_test`
   - Press Enter
   - Expected behavior: Firmware tests Lane 2 display module
2. **Visual inspection of Lane 2 display**:
   - Observe all five 7-segment digits in the right display module
   - Expected: All segments responsive (same pattern as Lane 1 test)
   - Record observed pattern: _________________
   - **PASS**: All 5 digits respond ☐ FAIL: Missing segments
3. **Test decimal point (Lane 2)**:
   - Expected: Decimal point illuminates (if implemented)
   - Observation: ☐ Functional **PASS** ☐ Not illuminating **FAIL**
4. **Record test result**:
   - Lane 2 display fully operational: ☐ **PASS** ☐ FAIL
   - Multiplexing properly implemented: ☐ **PASS** ☐ FAIL

**Pass Criteria**:

- ✓ All 5 digit positions on Lane 2 display respond to firmware
- ✓ Display segments illuminate without dead segments
- ✓ Decimal point responds (if present)
- ✓ Multiplexing timing comparable to Lane 1

---

### 5.7 Serial Data Output Test

**Objective**: Verify firmware transmits timing data and status messages via RS-232 serial interface to Race Manager.

**Circuit Reference**: LogicLevelShifter_sch.kicad_sch (level shifter from 3.3V logic to ±12V RS-232)

**Test Steps**:

1. **Monitor serial output**:
   - Keep Serial Monitor open and observe
   - Firmware should periodically transmit status messages or test results
   - Expected output examples:
     - `Sensor readings: Lane1=HIGH Lane2=HIGH`
     - `Time 1: 00.000 Time 2: 00.000`
     - Test command acknowledgments
2. **Verify readable output**:
   - All transmitted data should be ASCII text
   - No corrupted characters (special symbols that indicate baud rate mismatch)
   - Record sample output: _________________
   - **PASS**: Output readable and correctly formatted ☐ FAIL: Corrupted characters
3. **Test command echo (optional)**:
   - Send a command (e.g., `sensor_test`)
   - Expected: Command echoed back in Serial Monitor
   - Observation: ☐ Echo received **PASS** ☐ No echo **FAIL**
4. **Verify no data loss**:
   - Run a test sequence and observe Serial Monitor for 30 seconds
   - Expected: No missing lines or dropped characters
   - Record observations: _________________
   - ☐ No data loss detected **PASS** ☐ Characters missing **FAIL**

**Pass Criteria**:

- ✓ ASCII data transmitted without corruption
- ✓ All messages readable at 9600 baud
- ✓ No character loss or framing errors
- ✓ Serial communication stable for extended periods (30+ seconds)

---

### 5.8 Extended Sensor and Display Functional Test

**Objective**: Verify that sensors and displays work correctly together under sustained operation.

**Test Steps**:

1. **Start combined test**:
   - Type: `combined_test` or run sensor_test and observe display updates
   - Duration: Run for 60 seconds minimum
2. **Perform sensor activity**:
   - While test runs, periodically block Lane 1 and Lane 2 sensors
   - Some firmware versions may update display based on sensor input (e.g., increment time counter)
3. **Monitor for stability**:
   - Ensure no display segments fail or become intermittent
   - Ensure sensors respond consistently throughout test
   - Watch for any serial output errors or restarts
   - Record observations: _________________
4. **Stop test**:
   - Type: `end_test` or equivalent command
   - Expected: Firmware cleanly exits test mode
   - Record: ☐ Test exited normally **PASS** ☐ Hung or froze **FAIL**

**Pass Criteria**:

- ✓ No display segment failures during 60-second test
- ✓ Consistent sensor response throughout
- ✓ No serial communication errors
- ✓ Firmware maintains stable operation without reset

---

## 6. Test Teardown and Cleanup

1. **Disable test mode**:
   - Type: `end_test` or press Ctrl+C in Serial Monitor
   - Wait for command prompt to return
2. **Power down safely**:
   - Close Serial Monitor
   - Disconnect USB cable from Arduino (wait 2 seconds for USB power to discharge)
   - Turn OFF external 12V power supply
   - Wait 5 seconds for circuit discharge
3. **Disconnect peripherals**:
   - Disconnect optical sensors from J1
   - Disconnect display modules from J4 and J5
   - Disconnect 12V power leads from J3
4. **Board inspection**:
   - Visually inspect PCB for any discoloration, burn marks, or component damage
   - Check for any loose components or connector misalignment
   - Verify no solder splashes or cold joints introduced during testing
5. **Store test equipment**:
   - Disconnect USB cable
   - Store Arduino Nano in ESD-protective packaging if not immediately reused
   - Return optical sensors and power supply to proper storage

---

## 7. Test Results Summary

| Test Section | Description | Status | Pass/Fail | Notes | Technician | Date |
| --- | --- | --- | --- | --- | --- | --- |
| 5.1 | Serial Communication | ☐ Complete | ☐ P ☐ F | | | |
| 5.2 | Lane 1 Sensor Input | ☐ Complete | ☐ P ☐ F | All cycles passed | | |
| 5.3 | Lane 2 Sensor Input | ☐ Complete | ☐ P ☐ F | All cycles passed | | |
| 5.4 | Dual Sensor Simultaneous | ☐ Complete | ☐ P ☐ F | No crosstalk | | |
| 5.5 | Lane 1 Display Control | ☐ Complete | ☐ P ☐ F | All 5 digits functional | | |
| 5.6 | Lane 2 Display Control | ☐ Complete | ☐ P ☐ F | All 5 digits functional | | |
| 5.7 | Serial Data Output | ☐ Complete | ☐ P ☐ F | No data loss | | |
| 5.8 | Sustained Operation (60s) | ☐ Complete | ☐ P ☐ F | Stable throughout | | |

---

**Overall Status**: ☐ PASS ☐ CONDITIONAL ☐ FAIL

**Anomalies/Rework Required**:

---

**Test Conducted**: Date _________ Time _________

**Technician**: _________________________

**QA Review**: _________________________

---

## 8. Review Summary and Recommendations

### 8.1 Test Protocol Strengths

✓ **Systematic progression**: Tests begin with basic serial communication and progress through sensors, displays, and integrated operation  
✓ **Hardware correlation**: Each test references specific schematic components (Schmitt triggers, decoders, level shifters)  
✓ **Practical measurements**: Uses Serial Monitor output for verification (no specialized test equipment required)  
✓ **Pass/fail criteria clearly defined**: Expected output and acceptable timing ranges specified  
✓ **Hobbyist-appropriate**: Simple, achievable tests without complex instrumentation  

### 8.2 Protocol Improvements for Future Revision

1. **Add display pattern definitions**: Document expected digit patterns (e.g., "test will display 0-9 sequence")
2. **Timing tolerance specifications**: Add acceptable response time bounds for each sensor state change
3. **Command reference appendix**: List all expected Serial Monitor commands and expected responses
4. **Troubleshooting guide**: Add common failures and diagnostic steps:
   - Serial Monitor shows no output → Check USB drivers, reload firmware
   - Display segments do not light → Check 74HCT238 control lines, verify +5V supply
   - Sensors not responding → Check Schmitt trigger IC, verify sensor connections
   - Serial output corrupted → Check baud rate, verify USB cable quality
5. **Performance benchmarks** (optional): Record firmware execution timing:
   - Serial monitor response latency: < 50ms
   - Display refresh rate: > 30Hz
   - Sensor polling rate: > 100Hz

### 8.3 Functional Test vs. Peripheral Test Relationship

| Aspect | Peripheral Test (Hardware) | Functional Test (Firmware) |
| --- | --- | --- |
| **Focus** | Static voltages, continuity, signal presence | Dynamic firmware control, real-time I/O |
| **Power** | Voltages measured, minimal current draw | Full operation with sensor/display loads |
| **Equipment** | Multimeter, optical sensors | Arduino IDE, Serial Monitor |
| **Duration** | 30-45 minutes | 45-60 minutes |
| **Success** | All voltages in range, no shorts | All firmware commands execute, I/O responds |

Both tests must pass before proceeding to system integration testing.

### 8.4 Next Steps After Functional Test

Upon successful completion of this functional test:

1. **System Integration Test** (future):
   - Connect actual finish-line optical sensors
   - Integrate display modules into physical housing
   - Test complete timing capture workflow
   - Validate serial communication with Race Manager controller

2. **User Acceptance Test**:
   - Test in actual derby track environment
   - Verify timing accuracy with reference standard
   - Confirm all user-facing displays function correctly

---

## Appendix A: Serial Command Reference

### Sensor Testing
- `sensor_test` - Start continuous sensor monitoring (Lane 1 and Lane 2 states)

### Display Testing  
- `display1_test` - Cycle through 0-9 patterns on all 5 digits (Lane 1 / Left display)
- `display2_test` - Cycle through 0-9 patterns on all 5 digits (Lane 2 / Right display)

### Combined Testing
- `combined_test` - Run sensors and displays together for 60 seconds
- `end_test` - Exit any active test mode and return to command prompt

---

## Appendix B: Connector Quick Reference

| Connector | Purpose | Pin Count | Signals |
| --- | --- | --- | --- |
| **J3** | 12V Power Input | 2 | +12V, GND |
| **J1** | Optical Sensors | 4 | +12V (sensor supply), Lane 1, Lane 2, GND |
| **J6** | Display Power | 4 | +12V, GND (x2), +5V |
| **J5** | Lane 1 Display Output | 10 | 7-segment and DP data lines |
| **J4** | Lane 2 Display Output | 10 | 7-segment and DP data lines |
| **J2** | Serial Communications | 3 | TXD, RXD, GND to Race Manager |

---

## Appendix C: Diagnostics Quick Reference

**No startup message?**
- Check USB drivers
- Try different COM port
- Reload firmware and power-cycle Arduino

**Sensors not responding?**
- Verify Lane 1 and Lane 2 sensors are connected to J1
- Check Schmitt trigger IC (U3: 74LVC2G14) for proper power
- Measure sensor input voltage: should toggle between 0V and 3.3V
- Verify pull-up resistors are installed

**Display segments dark?**  
- Check 74HCT238 decoder (U4) control lines (A0, A1, A2)
- Verify 74HCT244 output buffers have +5V supply
- Check segment data lines with multimeter for continuity
- Confirm LED display modules have adequate +5V supply

**Serial output corrupted?**
- Verify baud rate set to 9600 baud
- Check USB cable integrity
- Slow down command entry rate
- Verify logic level shifter IC connections (RS-232 level conversion)
