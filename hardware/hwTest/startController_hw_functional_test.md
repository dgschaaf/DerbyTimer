# Start Controller Hardware Functional Test

## 1. Objective

The purpose of this test is to validate that the Start Controller PCB firmware executes correctly on the Arduino microprocessor and that all connected peripherals (pushbuttons, gate electromagnets, solenoid, and shift register-controlled lights) respond to software commands as expected. This test verifies active firmware control of the circuit, following successful completion of the peripheral hardware test (static continuity and voltage verification).

**Scope**:

- Arduino Nano properly installed on PCB
- All peripheral connections verified (buttons, gates, solenoid, lights)
- Firmware loaded and Serial Monitor communication established
- **Includes**: Functional testing with active firmware control of buttons, gate circuits, and light outputs
- **NOT included**: Timing synchronization with Finish Controller or complete race sequence (deferred to system integration testing)

**Success Criteria**: All buttons report correct state changes, gate electromagnets energize when commanded, solenoid activates and releases cleanly, shift register outputs control lights as expected, serial communication functions properly.

---

## 2. Background

The Start Controller PCB manages race start gate mechanisms and visual countdown signals. The firmware running on the Arduino Nano microprocessor controls:

- **Button Input Monitoring**: Reads four momentary pushbutton inputs (Start, Mode, Left trigger, Right trigger) through debouncing circuitry (74HC7014 Schmitt trigger buffer)
- **Gate Electromagnet Control**: Activates left and right electromagnets to hold/release race starting gates via driver circuits (MOSFETs in GateControl.kicad_sch)
- **Solenoid Discharge**: Energizes a return solenoid to reset gates between races (Solenoid_Control.kicad_sch)
- **Christmas Tree Light Sequencing**: Controls countdown light patterns via 74HC595 shift register (Light_Control.kicad_sch) - blue, yellow (3-level), green, and red lights
- **Serial Communication**: Sends status messages and receives commands from Race Manager via logic-level-shifted RS-232 interface

This functional test validates that the firmware correctly reads button states, activates gate and solenoid circuits, controls shift register light outputs, and maintains proper serial communication.

---

## 3. Equipment Required

- Laptop running Arduino IDE (v1.8.13 or later)
- USB 2.0 cable (USB Type-A to Micro-B for Arduino Nano)
- Arduino Nano or compatible (ATmega328 microprocessor)
- External 12V regulated power supply (2A minimum capacity, with current limiting enabled)
- Four momentary pushbutton switch simulators (or actual pushbutton switches)
- Solenoid and electromagnet test load (or current-limiting resistors to simulate load)
- Serial terminal monitor (Arduino IDE Serial Monitor or equivalent)
- Jumper wires (22 AWG) for test connections (optional)

---

## 4. Pre-Test Setup and Safety

### 4.1 Hardware Verification

1. **Peripheral test completion**: Confirm that the Start Controller peripheral hardware test (Section 5 of startController_hw_peripheral_test.md) has been successfully completed and documented
2. **Component installation verification**:
   - Arduino Nano is fully seated in the microprocessor headers (no lifted pins or rotation)
   - All button, gate, solenoid, and light connectors are securely connected
   - No visible damage, bent leads, or solder bridges
3. **Power supply safety**:
   - External 12V power supply set to 12.0V ± 0.3V
   - Current limiting set to 2.0A minimum
   - Power supply remains **OFF** until directed in test steps

### 4.2 Firmware Loading

1. Open Arduino IDE on laptop
2. Open File → Examples → or load startController_hw_functional_test.ino
3. Select Tools → Board: "Arduino Nano"
4. Select Tools → Port: "COM[X]" (appropriate USB serial port)
5. Verify baud rate is set to **9600 baud**
6. Click Upload button; wait for "Done uploading" message
7. Do **not** open Serial Monitor yet—wait for test procedure

---

## 5. Test Procedure

### 5.1 Serial Monitor Communication Verification

**Objective**: Verify that the Arduino Nano boots correctly and establishes Serial Monitor communication.

**Test Steps**:

1. **Connect USB**: Connect USB cable from laptop to Arduino Nano Micro-B connector
2. **Open Serial Monitor**:
   - Arduino IDE → Tools → Serial Monitor
   - Set baud rate to **9600 baud**
   - Set line ending to "Newline"
3. **Wait for startup message**:
   - Expected output: `Start Controller Functional Test Starting...` or `Start Controller Test - Ready`
   - If no message appears: Check USB port selection and reload firmware
   - Record: ☐ Startup message received **PASS** ☐ FAIL
4. **Command prompt ready**:
   - Expected output: Command prompt displayed (e.g., `Ready for commands...`)
   - Record: ☐ Command prompt visible **PASS** ☐ FAIL

**Pass Criteria**:

- ✓ Serial communication established at 9600 baud
- ✓ Startup message received within 5 seconds
- ✓ Command prompt displayed and ready for input

---

### 5.2 Button Input Test - All Four Buttons

**Objective**: Verify that firmware correctly reads all button inputs and reports state changes.

**Circuit Reference**: Button_Debounce.kicad_sch (74HC7014 Schmitt trigger conditioning)

**Test Steps**:

1. Ensure Serial Monitor is open with 9600 baud connection
2. **Activate continuous button monitoring**:
   - Type: `button_test`
   - Press Enter
   - Expected output: Firmware begins polling buttons and printing current state (e.g., `Start: LOW  Mode: LOW  Right: LOW  Left: LOW`)
3. **Test Start button**:
   - Do NOT press Start button
   - Expected output: `Start: LOW`
   - Record: ☐ Inactive state reads LOW **PASS** ☐ FAIL
4. **Test Start button press**:
   - Press and hold Start button
   - Expected output: `Start: HIGH` (or state changes to HIGH while held)
   - Record response time: < 100ms - **PASS** ☐ FAIL
5. **Test Start button release**:
   - Release Start button
   - Expected output: `Start: LOW` (returns to inactive state)
   - Record response time: < 100ms - **PASS** ☐ FAIL
6. **Test all four buttons in sequence**:
   - Repeat press/hold/release cycle for each button:
     - Mode button: LOW → HIGH → LOW ☐ **PASS** ☐ FAIL
     - Right trigger: LOW → HIGH → LOW ☐ **PASS** ☐ FAIL
     - Left trigger: LOW → HIGH → LOW ☐ **PASS** ☐ FAIL
7. **Test simultaneous button presses**:
   - Press Start and Mode buttons simultaneously
   - Expected: Both show HIGH independently
   - Record: ☐ No cross-talk **PASS** ☐ FAIL
   - Press Left and Right buttons simultaneously
   - Expected: Both show HIGH independently
   - Record: ☐ No cross-talk **PASS** ☐ FAIL

**Pass Criteria**:

- ✓ All four buttons transition cleanly between LOW and HIGH
- ✓ Transitions occur within < 100ms (no excessive debounce delay)
- ✓ No floating or intermediate voltage readings
- ✓ Simultaneous button presses registered independently (no mutual interference)

---

### 5.3 Gate Electromagnet Control Test

**Objective**: Verify firmware can energize and de-energize gate electromagnets via control signals.

**Circuit Reference**: GateControl.kicad_sch (MOSFET driver circuits for electromagnets)

**Precautions**:

- Ensure gate assembly is properly mounted and electromagnets cannot cause injury
- Be prepared for audible "click" from electromagnet engagement
- **Apply 12V power to J6 before starting this test**

**Test Steps**:

1. **Apply 12V power**:
   - Turn ON the external 12V power supply connected to J6
   - Wait 2 seconds for voltage regulators to stabilize
2. **Test Left Electromagnet**:
   - Type: `gate_left_test`
   - Press Enter
   - Expected: Left electromagnet activates immediately (audible click or magnetic pull sensation)
   - Expected duration: ~10 seconds before automatic deactivation
   - Expected output on Serial Monitor: Something like `Left gate ON` followed by `Left gate OFF`
   - Record activation: ☐ Electromagnet energized **PASS** ☐ No activation **FAIL**
   - Record release: ☐ Electromagnet released **PASS** ☐ Stuck energized **FAIL**
3. **Test Right Electromagnet**:
   - Type: `gate_right_test`
   - Press Enter
   - Expected: Right electromagnet activates for ~10 seconds
   - Expected output: `Right gate ON` then `Right gate OFF`
   - Record activation: ☐ Electromagnet energized **PASS** ☐ FAIL
   - Record release: ☐ Clean release **PASS** ☐ FAIL

**Pass Criteria**:

- ✓ Left electromagnet responds to control signal
- ✓ Right electromagnet responds to control signal
- ✓ Both electromagnets activate cleanly and release without stiction
- ✓ Activation/deactivation follows expected timing (10 seconds)

---

### 5.4 Solenoid Reset Control Test

**Objective**: Verify firmware can energize the solenoid for gate reset function.

**Circuit Reference**: Solenoid_Control.kicad_sch

**Precautions**:

- Solenoid may be loud when energized; expect audible "click" or vibration
- Ensure solenoid is not mechanically obstructed

**Test Steps**:

1. Ensure 12V power remains ON
2. **Activate solenoid reset**:
   - Type: `gate_reset_test`
   - Press Enter
   - Expected: Solenoid activates with audible click/vibration
   - Expected duration: ~3 seconds before automatic deactivation
   - Expected output: `Solenoid ON` followed by `Solenoid OFF`
3. **Observe solenoid behavior**:
   - Expected: Clean activation (sharp click), smooth deactivation
   - Record activation: ☐ Energized **PASS** ☐ No activation **FAIL**
   - Record release: ☐ Clean release **PASS** ☐ Stuck **FAIL**
4. **Repeat solenoid test**:
   - Run `gate_reset_test` two additional times to confirm consistent behavior
   - Cycle 1: ☐ **PASS** ☐ FAIL
   - Cycle 2: ☐ **PASS** ☐ FAIL

**Pass Criteria**:

- ✓ Solenoid energizes on command
- ✓ Solenoid releases cleanly without stiction
- ✓ Behavior is consistent across multiple activation cycles (3+ cycles)
- ✓ Timing matches expected ~3 second duration

---

### 5.5 Light Output Control Test - Individual Lights

**Objective**: Verify firmware correctly controls individual light channels via shift register.

**Circuit Reference**: Light_Control.kicad_sch (74HC595 shift register, LED drive circuits)

**Test Steps**:

1. Ensure 12V power remains ON and Serial Monitor is open
2. **Test Blue Left Light**:
   - Type: `blue_left`
   - Press Enter
   - Expected: Left blue light illuminates (or LED indicator activates)
   - Expected duration: ~5 seconds
   - Expected output: Something like `Light: Blue Left` followed by `Done`
   - Record: ☐ Light illuminated **PASS** ☐ Did not light **FAIL**
3. **Test Blue Right Light**:
   - Type: `blue_right`
   - Expected: Right blue light illuminates for ~5 seconds
   - Record: ☐ Light illuminated **PASS** ☐ FAIL
4. **Test Yellow Lights** (top to bottom):
   - Type: `yellow_3` → Expect top yellow (3rd light) to illuminate
   - Record: ☐ Top yellow lit **PASS** ☐ FAIL
   - Type: `yellow_2` → Expect middle yellow (2nd light) to illuminate
   - Record: ☐ Middle yellow lit **PASS** ☐ FAIL
   - Type: `yellow_1` → Expect bottom yellow (1st light) to illuminate
   - Record: ☐ Bottom yellow lit **PASS** ☐ FAIL
5. **Test Green Light**:
   - Type: `green`
   - Expected: Green light illuminates for ~5 seconds
   - Record: ☐ Green lit **PASS** ☐ FAIL
6. **Test Red Lights**:
   - Type: `red_left` → Expect left red light to illuminate
   - Record: ☐ Left red lit **PASS** ☐ FAIL
   - Type: `red_right` → Expect right red light to illuminate
   - Record: ☐ Right red lit **PASS** ☐ FAIL

**Pass Criteria**:

- ✓ Each light command activates the correct light independently
- ✓ Light stays on for expected duration (~5 seconds)
- ✓ Light cleanly turns off after test duration
- ✓ No cross-talk between light channels (one light on does not affect others)

---

### 5.6 Light Sequence Pattern Test

**Objective**: Verify firmware executes full countdown light sequence (simulating race start sequence).

**Test Steps**:

1. **Activate full sequence**:
   - Type: `lights_all`
   - Press Enter
2. **Observe sequence**:
   - Expected: Lights cycle through countdown pattern (typical derby timing: blues → yellows → green, with optional red fault pattern)
   - Example sequence:
     - Blues illuminate (startup)
     - Yellow 3, Yellow 2, Yellow 1 light in sequence or together
     - Green light (race start signal)
     - Optional: Red pattern may indicate error condition
   - Record observed sequence: _______________________________
3. **Monitor serial output**:
   - Watch Serial Monitor for sequence announcements
   - Record output: _______________________________
4. **Visual timing**:
   - Total sequence duration: Expected 15-30 seconds (depends on firmware design)
   - Record duration: ____________ seconds
   - Record: ☐ Sequence completed normally **PASS** ☐ Sequence aborted **FAIL**

**Pass Criteria**:

- ✓ Full light sequence executes without interruption
- ✓ Lights follow expected countdown pattern
- ✓ Timing matches design intent
- ✓ Serial communication confirms each step

---

### 5.7 Serial Communication Stability Test

**Objective**: Verify serial communication remains stable during extended test operation.

**Test Steps**:

1. **Run extended sensor and output commands**:
   - Type: `button_test`
   - For 30 seconds, perform the following repeatedly:
     - Press buttons
     - Watch serial output for correct state display
   - Record behavior: _______________________________
2. **Monitor for data corruption**:
   - Watch Serial Monitor for any character corruption or garbled text
   - Expected: All output readable and properly formatted
   - Record observations: ☐ No corruption **PASS** ☐ Garbled text **FAIL**
3. **Test command responsiveness**:
   - Send multiple commands (e.g., `yellow_1`, `yellow_2`, `yellow_3`, `green`) in rapid succession
   - Expected: All commands execute in order without missing any
   - Record: ☐ All commands executed **PASS** ☐ Command missed **FAIL**
4. **Verify no resets**:
   - Observe for any unexpected startup messages or resets
   - Record: ☐ No resets **PASS** ☐ Unexpected reset occurred **FAIL**

**Pass Criteria**:

- ✓ Serial data transmitted without corruption
- ✓ No character loss during command sequences
- ✓ Firmware responsive to commands for 30+ seconds
- ✓ No unexpected resets or lockups

---

### 5.8 Extended Functional Integration Test

**Objective**: Verify all systems work correctly together under sustained operation.

**Test Steps**:

1. **Perform consolidated test sequence**:
   - Run 60-second test combining buttons, gates, solenoid, and lights
   - Sequence:
     - Press Start button → Observe `Start: HIGH` in output
     - Wait 2 seconds
     - Type: `gate_left_test` → Left electromagnet activates
     - Type: `lights_all` → Full light sequence runs
     - Press buttons during light sequence → Buttons still respond
     - Type: `gate_right_test` → Right electromagnet activates
     - Type: `gate_reset_test` → Solenoid activates
2. **Monitor for stability**:
   - No component overheating
   - No excessive current draw
   - Serial communication stable throughout
   - Record observations: _______________________________
3. **Stop test**:
   - Type: `end_test` or any unknown command to reset
   - Expected: Firmware returns to ready state
   - Record: ☐ Normal exit **PASS** ☐ Hung/froze **FAIL**

**Pass Criteria**:

- ✓ Buttons, gates, solenoid, and lights all respond correctly
- ✓ No device failures during 60-second continuous operation
- ✓ No serial communication errors
- ✓ Firmware stable and responsive throughout test

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
   - Disconnect button connectors from J7-J10
   - Disconnect gate assembly from J2
   - Disconnect light connectors from J3 and J4
   - Disconnect 12V power leads from J6
4. **Board inspection**:
   - Visually inspect PCB for any discoloration, burn marks, or component damage
   - Check for any loose components or connector misalignment
   - Verify no solder splashes or cold joints introduced during testing
5. **Store test equipment**:
   - Disconnect USB cable
   - Store Arduino Nano in ESD-protective packaging
   - Return buttons, gate assembly, and power supply to proper storage

---

## 7. Test Results Summary

| Test Section | Description | Status | Pass/Fail | Notes | Technician | Date |
| --- | --- | --- | --- | --- | --- | --- |
| 5.1 | Serial Communication | ☐ Complete | ☐ P ☐ F | | | |
| 5.2 | Button Inputs (All 4) | ☐ Complete | ☐ P ☐ F | No cross-talk | | |
| 5.3 | Gate Electromagnets | ☐ Complete | ☐ P ☐ F | L/R gates functional | | |
| 5.4 | Solenoid Reset | ☐ Complete | ☐ P ☐ F | 3 activation cycles passed | | |
| 5.5 | Individual Light Outputs | ☐ Complete | ☐ P ☐ F | All 8 lights functional | | |
| 5.6 | Light Sequence Pattern | ☐ Complete | ☐ P ☐ F | Full sequence complete | | |
| 5.7 | Serial Data Stability | ☐ Complete | ☐ P ☐ F | No corruption (30s) | | |
| 5.8 | Extended Integration (60s) | ☐ Complete | ☐ P ☐ F | All subsystems stable | | |

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

✓ **Systematic progression**: Tests begin with basic serial communication, progress through individual peripherals, then combined operation  
✓ **Hardware correlation**: Each test references specific schematic components (Schmitt triggers, MOSFET drivers, shift register)  
✓ **Practical measurements**: Uses Serial Monitor output for verification (no specialized test equipment required)  
✓ **Pass/fail criteria clearly defined**: Expected outputs and acceptable timing ranges specified  
✓ **Hobbyist-appropriate**: Simple, achievable tests using standard Arduino tools  

### 8.2 Protocol Improvements for Future Revision

1. **Add light bit-pattern documentation**: Document shift register bit assignments for each light
2. **Timing tolerance specifications**: Add acceptable response time bounds for each control command
3. **Current draw monitoring** (optional): Measure peak current during gate/solenoid activation
4. **Visual indicators**: Note LED status indicators on PCB that can be observed during test (power, activity)
5. **Troubleshooting guide**: Add common failures and diagnostic steps:
   - Button not responding → Check 74HC7014 debounce circuit connections
   - Gate not energizing → Check MOSFET driver circuit, verify +12V supply
   - Solenoid not activating → Check solenoid driver circuit, verify gate reset circuit
   - Lights not illuminating → Check 74HC595 shift register clock/data/latch lines
   - Garbled serial output → Check baud rate, verify USB cable quality

### 8.3 Functional Test vs. Peripheral Test Relationship

| Aspect | Peripheral Test (Hardware) | Functional Test (Firmware) |
| --- | --- | --- |
| **Focus** | Static voltages, continuity, signal presence | Dynamic firmware control, real-time I/O |
| **Power** | Voltages measured, minimal current draw | Full operation with gate/solenoid/light loads |
| **Equipment** | Multimeter, pushbutton switches | Arduino IDE, Serial Monitor |
| **Duration** | 30-45 minutes | 60-90 minutes |
| **Success** | All voltages in range, no shorts | All firmware commands execute, I/O responds |

Both tests must pass before proceeding to system integration testing.

### 8.4 Next Steps After Functional Test

Upon successful completion of this functional test:

1. **System Integration Test** (future):
   - Connect Start Controller to Finish Controller via serial link
   - Integrate button triggers into actual race sequencing
   - Validate gate timing and solenoid response
   - Test complete start-to-finish race sequence

2. **User Acceptance Test**:
   - Test in actual derby track environment
   - Verify button responsiveness with race operators
   - Confirm lights visible under outdoor conditions
   - Validate complete race timing workflow

---

## Appendix A: Command Reference

### Button and Status Testing
- `button_test` - Start continuous button state monitoring (Start, Mode, Left, Right)
- `end_test` - Exit any active test mode

### Gate and Solenoid Control
- `gate_left_test` - Activate left gate electromagnet (~10 seconds)
- `gate_right_test` - Activate right gate electromagnet (~10 seconds)
- `gate_reset_test` - Activate solenoid for gate reset (~3 seconds)

### Individual Light Control
- `blue_left` - Illuminate left blue light (~5 seconds)
- `blue_right` - Illuminate right blue light (~5 seconds)
- `yellow_1` - Illuminate bottom yellow light (~5 seconds)
- `yellow_2` - Illuminate middle yellow light (~5 seconds)
- `yellow_3` - Illuminate top yellow light (~5 seconds)
- `green` - Illuminate green light (~5 seconds)
- `red_left` - Illuminate left red light (~5 seconds)
- `red_right` - Illuminate right red light (~5 seconds)

### Sequence Control
- `lights_all` - Run full countdown sequence (all lights in pattern, ~15-30 seconds)

---

## Appendix B: Connector Quick Reference

| Connector | Purpose | Pin Count | Primary Signals |
| --- | --- | --- | --- |
| **J6** | 12V Power Input | 2 | +12V, GND |
| **J7** | Start Button Input | 2 | Signal, GND |
| **J8** | Mode Button Input | 2 | Signal, GND |
| **J9** | Right Trigger Button | 2 | Signal, GND |
| **J10** | Left Trigger Button | 2 | Signal, GND |
| **J2** | Gate & Solenoid Output | 3 | Gate Left, Gate Right, Solenoid |
| **J3** | Right Light Control | 3 | Shift Reg control signals |
| **J4** | Left Light Control | 3 | Shift Reg control signals |

---

## Appendix C: Diagnostics Flowchart

```
START POWER UP
    ↓
[Startup message?] → NO → Check USB driver, reload firmware
    ↓ YES
[Command prompt visible?] → NO → Check Serial Monitor baud rate (9600)
    ↓ YES
RUN: button_test
    ↓
[Buttons respond 0-100ms?] → NO → Check Button_Debounce circuit (74HC7014)
    ↓ YES
RUN: gate_left_test
    ↓
[Electromagnet activates?] → NO → Check MOSFET driver, verify +12V
    ↓ YES
RUN: gate_right_test
    ↓
[Electromagnet activates?] → NO → Check MOSFET driver, verify +5V rail
    ↓ YES
RUN: gate_reset_test
    ↓
[Solenoid clicks?] → NO → Check Solenoid_Control circuit, verify driver output
    ↓ YES
RUN: blue_left
    ↓
[Light illuminates?] → NO → Check 74HC595 shift register clock/data/latch
    ↓ YES
RUN: lights_all
    ↓
[Full sequence completes?] → NO → Check shift register timing or serial glitch
    ↓ YES
OVERALL: PASS ✓
```

**Troubleshooting Quick Tips**:
- No serial output → Check USB cable, try different COM port, toggle power
- Buttons don't respond → Measure voltage on debounce output (should toggle 0-5V)
- Gates won't activate → Verify +12V supply under load, check MOSFET gate voltage
- Lights don't work → Confirm shift register latch signal (high pulse to load data)
- Corrupted serial output → Slow down command entry rate, verify baud rate match

