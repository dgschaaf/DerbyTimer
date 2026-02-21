# Start Controller Hardware Peripheral Test

## 1. Objective

The purpose of this test is to perform a comprehensive checkout of the Start Controller PCB prior to integration with the microprocessor. This test validates all external peripheral connections, power supply voltages, button input circuits, gate control circuits with electromagnets, and solenoid functionality.

**Scope**: This test requires a fully populated and assembled PCB with all external peripherals properly connected and external power supplied.

---

## 2. Background

The Start Controller board is responsible for:

- Managing four momentary pushbutton inputs (Start, Mode, Left trigger, Right trigger) with debouncing circuitry (74HC7014 Schmitt trigger buffer)
- Controlling gate electromagnets (Gate Left and Gate Right) for race gate operation
- Controlling a return solenoid for gate reset
- Interfacing with left and right Christmas tree light assemblies via shift register control
- Interfacing with optional left and right RFID modules
- Providing regulated power supplies: +12V, +5V, and +3.3V

This peripheral test validates basic board functionality at the component level before microprocessor integration.

---

## 3. Equipment Required

- Digital multimeter (DC voltage measurement capability)
- External 12 VDC regulated power supply (minimum 2A capacity)
- Four momentary pushbutton switches (SPST)
- Gate control assembly with two electromagnets and one return solenoid
- Left and right Christmas tree light assemblies (LED arrays)
- Left and right RFID modules (optional for this test)
- Jumper wires (22 AWG) for temporary connections
- ESD protection mat and wrist strap

---

## 4. Setup and Configuration

### Pre-Test Verification

1. Confirm PCB is fully mounted in its housing and all solder joints are clean and complete
2. Verify all decoupling capacitors are installed on power and logic IC rails
3. Confirm external connectors are properly soldered and have no cold joints
4. Ensure all jumper selections are correct per board configuration

### Peripheral Connections

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
6. **RFID Modules** (optional): Connect to connector **J5** (if equipped)

### Power Supply Setup

- Ensure the external 12V power supply is powered OFF before proceeding
- Verify supply voltage is set to exactly 12V ± 0.5V
- Confirm current limiting is enabled (if available)

---

## 5. Test Procedure

### 5.1 Power Supply Verification Test

**Purpose**: Verify all voltage regulators are functioning and stable
**Steps**:

1. Connect external 12V power supply to connector J6
2. Set multimeter to DC voltage mode with 20V range
3. **CRITICAL SAFETY STEP**: Before powering on, visually inspect the board for:
   - Solder bridges between tracks
   - Reversed polarity components
   - Missing or misaligned ICs
4. Turn ON the external 12V power supply
5. **IMMEDIATE OBSERVATION**: If any component makes unusual noise, emits odor, exhibits smoke, or becomes excessively hot, turn OFF the power supply immediately
6. Wait 10 seconds for voltage regulators to stabilize
7. Place multimeter ground probe on J12 pin 14 (GND reference point)
8. Measure voltage at J6 input:
   - Place test probe on positive terminal of J6
   - Expected reading: **12.0V ± 0.5V**
   - Record: _______ V
9. Measure +5V regulated output:
   - Place test probe on J2 pin 1
   - Expected reading: **5.0V ± 0.2V**
   - Record: _______ V
10. Measure +3.3V regulated output:
    - Place test probe on J1 pin 1
    - Expected reading: **3.3V ± 0.1V**
    - Record: _______ V
11. If voltages are outside tolerance, STOP testing and investigate voltage regulator circuits (Power.kicad_sch)

**Pass Criteria**: All three voltage measurements within specified tolerances

---

### 5.2 Button Input Test (Debounce Circuit Verification)

**Purpose**: Verify button debouncing circuitry (74HC7014 Schmitt trigger) and input signal conditioning
**Circuit Reference**: Button_Debounce.kicad_sch
**Steps**:

1. Ensure power supply is ON and voltages verified as per 5.1
2. Set multimeter to DC voltage mode with 20V range
3. Place multimeter ground probe on J12 pin 14 (GND reference)

#### Test Start Trigger (J7)

1. Place test probe on J12 pin 11 (Start trigger output)
2. Confirm button is NOT pressed:
   - Expected reading: **0V (LOW)**
   - Record: _______ V
3. Press and hold Start trigger button
   - Expected reading: **5V (HIGH)**
   - Record: _______ V
4. Release button:
   - Expected reading: **0V (LOW)**
   - Record: _______ V

#### Test Mode Trigger (J8)

1. Place test probe on J12 pin 10 (Mode trigger output)
2. Confirm button is NOT pressed:
   - Expected reading: **0V (LOW)**
   - Record: _______ V
3. Press and hold Mode trigger button
    - Expected reading: **5V (HIGH)**
    - Record: _______ V
4. Release button:
    - Expected reading: **0V (LOW)**
    - Record: _______ V

#### Test Right Trigger (J9)

1. Place test probe on J12 pin 9 (Right trigger output)
2. Confirm button is NOT pressed:
    - Expected reading: **0V (LOW)**
    - Record: _______ V
3. Press and hold Right trigger button
    - Expected reading: **5V (HIGH)**
    - Record: _______ V
4. Release button:
    - Expected reading: **0V (LOW)**
    - Record: _______ V

#### Test Left Trigger (J10)

1. Place test probe on J12 pin 8 (Left trigger output)
2. Confirm button is NOT pressed:
    - Expected reading: **0V (LOW)**
    - Record: _______ V
3. Press and hold Left trigger button
    - Expected reading: **5V (HIGH)**
    - Record: _______ V
4. Release button:
    - Expected reading: **0V (LOW)**
    - Record: _______ V

**Pass Criteria**: All four buttons transition cleanly between 0V and 5V with no floating or intermediate voltages; transitions are crisp without oscillation

---

### 5.3 Gate Electromagnet and Solenoid Control Test

**Purpose**: Verify power distribution to gate electromagnets and solenoid circuits capable of energizing external loads
**Circuit Reference**: GateControl.kicad_sch and Solenoid_Control.kicad_sch
**Precautions**:

- Ensure gate assembly is properly mounted and electromagnets cannot cause injury
- Keep fingers and loose clothing away from moving solenoid
- Be prepared for audible "click" from electromagnet engagement

**Steps**:

1. Ensure power supply is ON and voltages verified as per 5.1
2. Note: Gate control circuits are designed to be driven by microprocessor GPIO. For this PCB checkout test, we verify power availability and use jumper wire to simulate control signal
3. Set multimeter to DC voltage mode with 20V range
4. Place multimeter ground probe on J12 pin 14 (GND reference)

#### Verify Gate Right Control Power

1. Place test probe on J12 pin 4 (Gate Right control output)
2. With no control signal applied:
   - Expected reading: **0V (floating)**
   - Record: _______ V
3. Apply +5V control signal using jumper wire from J2 pin 1 to J12 pin 4
4. Verify Gate Right electromagnet energizes (should hear/feel slight magnetic pull)
5. With control signal applied, measure voltage:
   - Expected reading: **~2.7V to 5.0V** (voltage drop across driver circuit)
   - Record: _______ V
6. Remove jumper wire - electromagnet should de-energize

#### Verify Gate Left Control Power

1. Place test probe on J12 pin 5 (Gate Left control output)
2. With no control signal applied:
    - Expected reading: **0V (floating)**
    - Record: _______ V
3. Apply +5V control signal using jumper wire from J2 pin 1 to J12 pin 5
4. Verify Gate Left electromagnet energizes (should hear/feel slight magnetic pull)
5. With control signal applied, measure voltage:
    - Expected reading: **~2.7V to 5.0V** (voltage drop across driver circuit)
    - Record: _______ V
6. Remove jumper wire - electromagnet should de-energize

#### Verify Solenoid Control Power

1. Place test probe on J12 pin 6 (Return solenoid control output)
2. With no control signal applied:
    - Expected reading: **0V (floating)**
    - Record: _______ V
3. Apply +5V control signal using jumper wire from J2 pin 1 to J12 pin 6
4. Verify return solenoid energizes (should hear/feel click or vibration)
5. With control signal applied, measure voltage:
    - Expected reading: **~2.7V to 5.0V** (voltage drop across driver circuit)
    - Record: _______ V
6. Remove jumper wire - solenoid should de-energize
7. Remove all jumper wires

**Pass Criteria**: All three electromagnet/solenoid circuits show proper response to control signal; no audible arcing or component overheating

---

### 5.4 Christmas Tree Light Control Test

**Purpose**: Verify shift register interface and output driver circuits for light control
**Circuit Reference**: Light_Control.kicad_sch
**Status**: This test requires firmware integration to control the 74HC514 shift register.
**Deferred Testing**:

- Shift register data/clock/latch signal routing
- Individual light channel on/off control
- Brightness level control (if PWM configured)

**Recommended Actions**:

- After microprocessor integration, verify shift register timing signals (CLK, LATCH, DATA)
- Test each light channel independently with logic analyzer
- Verify current drive capacity under loaded conditions
- Confirm no interference with 12V power lines when lights energize

---

### 5.5 RFID Module Interface Test

**Purpose**: Verify SPI communication lines and power delivery to RFID modules
**Status**: This test is deferred pending microprocessor firmware development
**Connector**: J5 (if equipped)
**Interface**: SPI (pins: MISO, MOSI, SCK, CS)
**Deferred Testing**:

- SPI clock signal presence and frequency
- MISO/MOSI data line voltage levels
- Chip Select (CS) signal toggling
- Module response to SPI queries

**Precautions**:

- RFID modules may be sensitive to ESD; use proper grounding
- Verify module supply voltage (typically 3.3V) before connection

**Recommended Actions**:

- After microprocessor integration, capture SPI waveforms with oscilloscope
- Verify slave select timing relative to data transfers
- Confirm antenna tuning with test reads/writes
- Test bidirectional communication with reference tag

---

## 6. Teardown

1. Turn OFF the external 12V power supply
2. Allow board to cool for 5 minutes
3. Remove all test equipment and jumper wires
4. Disconnect external peripherals (gate assembly, buttons, lights, RFID modules) in reverse order of connection
5. Visually inspect board for any damage or discoloration
6. Document any anomalies for rework

---

## 7. Test Results Summary

| Test Section | Status | Comments | Technician | Date |
| --- | --- | --- | --- | --- |
| 5.1 Power Supply | ☐ Pass ☐ Fail | | | |
| 5.2 Button Inputs | ☐ Pass ☐ Fail | | | |
| 5.3 Gate/Solenoid | ☐ Pass ☐ Fail | | | |
| 5.4 Light Control | ☐ Deferred | Requires firmware | | |
| 5.5 RFID Interface | ☐ Deferred | Requires firmware | | |

**Overall Status**: ☐ Pass ☐ Conditional ☐ Fail
**Notes / Action Items**:

---

---

---

---

## 8. Additional Recommendations for Improvement

### Design Verifications

1. **Measure capacitor voltage ripple**: Connect oscilloscope to +5V and +3.3V rails to verify noise < 100mV peak-to-peak under button switching
2. **Test reverse polarity protection**: Verify current limiting behavior if power supply polarity is accidentally reversed
3. **Button debounce verification**: Connect oscilloscope to button inputs to observe Schmitt trigger output for proper edge detection and debounce delay (typically 1-2ms for 74HC7014)

### Board Assembly Quality

1. Check all solder joints under magnification (10x) for:
   - Proper wetting and shiny appearance
   - No bridges between adjacent pins
   - No insufficient solder (cold solder joints)
2. Verify no component popcorning or lifting at pads
3. Ensure all silk-screen labels are legible and accurate

### Future Testing (Post-Firmware Integration)

1. **Dynamic Load Testing**: Add measured loads to gate electromagnets and measure voltage sag
2. **EMI/RFI Compliance**: Test for radiation compliance near 2.4GHz RFID frequencies
3. **Thermal Imaging**: Confirm no hot spots under sustained operation
4. **Stress Testing**: Run continuous button press/gate activation cycles for 1 hour
5. **Single Event Upset (SEU) Testing**: If radiation tolerance is a requirement

### Documentation Improvements

1. Add connector pinout table for quick reference during troubleshooting
2. Create quick-reference voltage table for all test nodes
3. Add schematic page references to each test section for technician convenience
4. Include expected current draw ranges for each subsystem
