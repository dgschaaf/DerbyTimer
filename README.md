# DerbyTimer

Pinewood derby timing and race track control system



## Start Controller System

Version: 1.0
Author: Darren Schaaf
Date: September 2025

### System Overview

The Start Controller manages the beginning of pinewood derby races, controlling starting gates, countdown lights, and car identification. It operates as an embedded system on an Arduino Nano with a custom PCB shield, communicating with a separate Finish Controller via serial UART.

### Core Capabilities

#### Race Management

* \*\*Four Race Modes\*\*: Gate Drop, Reaction Time, Pro Tree, and Dial-In
* \*\*Six Race States\*\*: Idle → Staging → Countdown → Racing → Complete
* \*\*Automated Sequencing\*\*: Handles complete race lifecycle with state-driven logic

#### Hardware Control

* \*\*Dual Starting Gates\*\*: Electromagnet-held gates with spring-loaded drop mechanism
* \*\*Return Solenoid\*\*: Automated gate reset system with 500ms activation window
* \*\*Christmas Tree Lights\*\*: Dual 6-light arrays (Blue, Yellow×3, Green, Red) via shift register
* \*\*RFID Car Detection\*\*: Dual RC522 modules for automatic car identification

#### User Interface

* \*\*Four Button Inputs\*\*: Start, Mode, Left Lane, Right Lane
* \*\*Visual Feedback\*\*: LED patterns indicate mode, state, and race status
* \*\*Foul Detection\*\*: Red light indication for false starts in reaction modes

### Technical Architecture

#### Module Structure

| Module | Purpose | Key Functions |
|--------|---------|---------------|
| \\\\\\\*\\\\\\\*startController\\\\\\\*\\\\\\\* | Main state machine and orchestration | `setup()`, `loop()`, state transitions |
| \\\\\\\*\\\\\\\*serialComm\\\\\\\*\\\\\\\* | UART communication protocol | `rxSerial()`, `txRaceState()`, ACK/NACK handling |
| \\\\\\\*\\\\\\\*lights\\\\\\\*\\\\\\\* | LED tree control via shift register | `updateLights()`, `buildLightConfig()`, `startBlink()` |
| \\\\\\\*\\\\\\\*gates\\\\\\\*\\\\\\\* | Electromagnet and solenoid control | `dropGate()`, `returnGates()` |
| \\\\\\\*\\\\\\\*buttons\\\\\\\*\\\\\\\* | Input debouncing and detection | `isStartPressed()`, `isModePressed()` |
| \\\\\\\*\\\\\\\*rfid\\\\\\\*\\\\\\\* | NFC car identification | `readTag()`, `setupRFID()` |
| \\\\\\\*\\\\\\\*globals\\\\\\\*\\\\\\\* | Shared enumerations and constants | Race states, modes, bit masks |

#### Communication Protocol

##### Message Types (14 total):

* State synchronization (MODE, STATE)
* Car identification (LEFT\_CAR\_ID, RIGHT\_CAR\_ID)
* Race events (START, REACT, FOUL, WINNER)
* Control flow (ACK, NACK, ERROR)

##### Reliability Features:

* ACK/NACK confirmation system
* 3-retry limit with 50ms timeout
* State verification before transitions

#### State Machine Design

IDLE ──\[Start]──> STAGING ──\[Start]──> COUNTDOWN ──\[Green]──> RACING ──\[Complete]──> COMPLETE
↑                   │                                                                    │
└───────\[Mode]──────┘                                                   \[Start/Display]─┘

##### State Characteristics:

* Entry actions (reset, initialization)
* Continuous actions (monitoring, updates)
* Exit actions (cleanup, transmission)
* Guarded transitions (prevent invalid states)

### Key Design Decisions

#### 1\. Modular Architecture

Each hardware subsystem has its own module with clear interfaces, enabling independent testing and modification.

#### 2\. Pragmatic Global State

Uses extern globals for state visibility, appropriate for embedded systems where debugging access is critical.

#### 3\. Hybrid Programming Style

Combines procedural functions with data structs, avoiding over-engineering while maintaining structure.

#### 4\. Timing Precision

* Microsecond precision (`micros()`) for reaction time measurement
* Millisecond precision (`millis()`) for UI and timeouts

#### 5\. Safety First

* Hardware timeouts on all actuators
* Foul detection in reaction modes
* State verification before transitions

### Performance Characteristics

| Metric | Value | Notes |
|--------|-------|-------|
| \\\\\\\*\\\\\\\*Serial Baud\\\\\\\*\\\\\\\* | 115,200 | High-speed UART |
| \\\\\\\*\\\\\\\*Reaction Precision\\\\\\\*\\\\\\\* | ±1 µs | Using micros() |
| \\\\\\\*\\\\\\\*Message Timeout\\\\\\\*\\\\\\\* | 50 ms | Balance between responsiveness and reliability |
| \\\\\\\*\\\\\\\*RFID Read Rate\\\\\\\*\\\\\\\* | 2 Hz | 500ms threshold between reads |
| \\\\\\\*\\\\\\\*Countdown Timing\\\\\\\*\\\\\\\* | 400-500 ms | Mode-dependent staging |

### Extension Points

#### For Finish Controller Integration

* Complete serial protocol implementation
* Symmetric message handling
* Time synchronization considerations

#### For Feature Enhancement

* Additional race modes via MODE enum extension
* Custom countdown sequences via configuration
* Enhanced error reporting through ERROR messages

### Deployment Notes

#### Hardware Requirements

* Arduino Nano (ATmega328P)
* Custom PCB shield (see schematic)
* External 5V regulator for electromagnets
* External 3.3V regulator for RFID modules
* 74HC595 shift register for lights

#### Pin Assignments

See pinout documentation for complete mapping. Note D18/D19 correction from original D11/D12 button assignments to avoid SPI conflicts.

#### Power Considerations

* 12V supply for electromagnets and lights
* 5V logic with external regulation
* 3.3V RFID with external regulation
* ~1A peak current during gate return

### Maintenance Guidelines

#### Adding Race Modes

1. Extend `raceMode` enum in globals.h
2. Add case to mode button handler
3. Define light pattern for mode indication
4. Implement countdown behavior if different

#### Debugging

* Serial output at 115,200 baud
* LED patterns indicate errors
* "future:" comments mark enhancement points
* State visibility through global examination

#### Testing Recommendations

1. Light test pattern on startup
2. Gate cycling verification
3. RFID read confirmation via LED feedback
4. Serial loopback testing
5. Timing accuracy validation

### Summary

The Start Controller represents a well-engineered embedded system that balances sophistication with maintainability. Its modular architecture, clear state machine, and robust communication protocol make it suitable for reliable race management while remaining accessible for hobbyist modification and debugging.





## Finish Controller System



This document summarizes the design of the finish controller firmware for a hobbyist Pinewood‑Derby timing system. The **finish controller** complements a separate start controller and reports race results to a future **race manager** (a Raspberry Pi via BLE). It measures finish times, combines them with start reaction times, determines the winner, and updates large seven‑segment displays. Where appropriate the design sacrifices complexity for clarity so that a hobbyist can understand and extend the code.



### Hardware Overview



* **Controller** – An **Arduino Nano 33 BLE** runs the finish controller firmware. It communicates with the **start controller** over a UART defined in *serialComm* and will later talk BLE to the race manager. Since the Nano 33 BLE supports interrupts on most pins, the exact sensor pins are flexible.
* **Finish sensors** – Each lane has an **SE61** optical sensor with an external pull‑up resistor. The signals pass through a **74HC14** Schmitt inverter on the shield to provide clean edges. A configurable *activeHigh* flag in the sensors module selects whether the interrupt triggers on a rising or falling edge. A minimum race time (*minRaceTimeUs*) filters out spurious triggers during the first 0.5 s.
* **Shift registers and display** – Two chained **74HC595** shift registers drive the displays. The **lower byte** (U3) holds the three address bits for a **74HC137** digit demultiplexer and lane/mode flags. The **upper byte** (U1) contains four decimal‑point flags (for left/right time/reaction) and the 4‑bit BCD digit value (AD0..AD3) connected to **MC14543B** BCD‑to‑7‑segment drivers. The demultiplexer selects one of five digit positions (tens, ones, tenths, hundredths, thousandths) on the left or right display.
* **Unspecified pins** – The shield provides latch and enable signals for the demultiplexer and drivers. The firmware defines *SHIFT\_DATA\_PIN*, *SHIFT\_CLK\_PIN* and *SHIFT\_LATCH\_PIN* for the **74HC595**; if additional control pins (e.g. *LE* on the demux) are required, they should be added to the code and toggled in *shiftOut16*().



### Sensor Handling



The *sensors* module encapsulates finish sensor handling:



* A *SensorConfig* structure defines the *leftPin*, *rightPin*, *activeHigh* polarity and time filters (*minRaceTimeUs* and *maxRaceTimeUs*). In this design *minRaceTimeUs* is 500 000 µs (0.5 s), and *maxRaceTimeUs* is 10 000 000 µs (10 s). You can update the pins without touching the rest of the code.
* *setupSensors*() configures the pin modes but does not attach interrupts. Interrupts are attached in *armSensors*() and detached in *disarmSensors*() to minimize spurious triggers when idle.
* When a race starts, *armSensors*(*startMicros*) records the absolute start time and attaches interrupts on the correct edge (rising if *activeHigh* is true, falling otherwise). The ISRs record the elapsed time (*micros() - start*) only once per lane. If the elapsed time is less than *minRaceTimeUs* the trigger is ignored (prevents false starts from mechanical bounce). After both lanes have been recorded or *maxRaceTimeUs* expires, the sensors are disarmed.
* Volatile flags *leftFinished* and *rightFinished* are exposed for polled access. Helper functions return the recorded microsecond times.



### Race Logic



Race state transitions are driven externally via messages from the start controller (see serialComm.h). The finish controller participates only when the state is *RACE\_RACING* or *RACE\_COMPLETE*:



*1. RACE\_RACING* – On first entry, the code records micros() as *raceStartUs*, calls *armSensors*(), and clears per‑race flags. Each loop iteration polls the finish flags via *isLeftFinished*()/*isRightFinished*() and records the sensor times. If *maxRaceTimeUs* expires before both lanes finish, missing lanes are assigned the max time. Once both times are available, the controller:

* Disarms the sensors.
* Retrieves reaction times (in microseconds) and foul flags from the start controller via the global rxLeftReactionTime, rxRightReactionTime, rxLeftFoul, rxRightFoul updated by rxSerial().
* Computes car times: for a foul start the reaction time is added to the race time; otherwise it is subtracted. After the arithmetic the code rounds the car time to the nearest millisecond ((value + 500)/1000\*1000) to align with the specification. Negative differences underflow to zero.
* Determines the winner by comparing the rounded car times. Ties are allowed.
* Stores all results in two RaceResults structures (leftResults and rightResults) and transitions the state to RACE\_COMPLETE.



2\. RACE\_COMPLETE – On first entry the controller builds a winner mask (bit 0=left, bit 1=right, bit 2=tie) and calls *txWinner*(*winnerMask*) to notify the start controller, which flashes lane lights accordingly. It then calls *displayRaceTimes*() to update both displays with the raw finish times (rounded to the nearest millisecond when presented). Subsequent operations depend on *MSG\_DISP\_ADVANCE* commands from the start controller:

* The first advance (for modes other than *MODE\_GATEDROP*) triggers displayReactionTimes(), showing reaction times on the displays.
* A second advance signals readiness to return to idle. The controller calls *txRaceState*(*RACE\_IDLE*) and, upon acknowledgment, resets its internal flags and returns to the idle state.
* Only the winner message is currently sent; in the future, car and reaction times will also be transmitted to the race manager via BLE.



### State and Mode Variables

*globals.h* defines enumerations *raceState* and *raceMode* and global variables *currentState*, *targetState*, *currentMode* and *targetMode*. The finish controller reads these to decide which state handler to run. The start controller sets *targetState* and *targetMode* via messages; the finish controller updates its *currentState* when appropriate. Additional global variables track fouls, reaction times and start times, although the finish controller primarily uses the values provided by *serialComm*.



### Display Design

The firmware drives two 5‑digit seven‑segment displays via a pair of chained 74HC595 shift registers. Each call to *updateDisplay(timeUs, isReaction, isLeft)* performs the following steps:

1. Round the time to the nearest millisecond and clamp values above 99.999 s to 99.999.
2. Extract five digits: tens of seconds, ones of seconds, tenths, hundredths and thousandths (e.g. 1.234 s → digits 0, 1, 2, 3, 4). The decimal point should appear between the ones and tenths positions.
3. For each digit build a 16‑bit pattern via *buildPattern()*:

* Bits \[0..2] – the digit index (A0..A2) for the demultiplexer.
* Bit 3 – selects time (0) or reaction (1).
* Bit 4 – selects left lane (0) or right lane (1).
* Bits 5..7 – unused; set to 0.
* Bits \[8..11] – decimal flags for the four displays. Four constants (D*ECIMAL\_LEFT\_TIME, DECIMAL\_RIGHT\_TIME, DECIMAL\_LEFT\_REACT, DECIMAL\_RIGHT\_REACT*) map to U1 QA..QD. When *showDecimal* is true (for digit index 1), the code sets the appropriate flag based on whether the current lane and mode represent a time or reaction display
* Bits \[12..15] – the 4‑bit BCD digit value (0–9) placed on AD0..AD3.

4\. Shift the pattern out MSB‑first into the **74HC595s** using *shiftOut16*(). After 16 clock pulses, the latch pin toggles high then low to transfer the outputs. Because each digit’s pattern is latched individually, no multiplexing delays are required. If your hardware uses additional latches (for the demultiplexer or **MC14543**) insert them in *shiftOut16*() as needed.



### Decimal and BCD Wiring

The exact wiring of the decimal flags and BCD outputs depends on the shield. The code assumes:

* *DECIMAL\_LEFT\_TIME* (U1 QA) controls the decimal point on the left race‑time display.
* *DECIMAL\_RIGHT\_TIME* (U1 QB) controls the decimal point on the right race‑time display.
* *DECIMAL\_LEFT\_REACT* (U1 QC) controls the decimal point on the left reaction‑time display.
* *DECIMAL\_RIGHT\_REACT* (U1 QD) controls the decimal point on the right reaction‑time display.



If your board routes these differently, update the constants in *finishController.cpp*. Similarly, verify which bits of the upper byte connect to the **MC14543** AD0..AD3 lines and adjust *buildPattern*() accordingly. The demultiplexer’s latch enable and chip enable pins may also require control; add additional *digitalWrite*() toggles around the shift/latch operations if necessary.



### Communication with Start Controller



The finish controller communicates with the start controller over a serial connection handled by *serialComm*. Key functions include:

* *rxSerial()* – Parses incoming messages, updates global variables (e.g. *rxLeftFoul*, *rxRightFoul*, *rxLeftReactionTime*, *rxRightReactionTime*, *rxDisplayAdvanceFlag*) and returns true when a complete message is received. Call this often in the main loop.
* *txWinner(uint8\_t winnerMask)* – Sends the winner message (*MSG\_WINNER*) to the start controller. Bits 0 and 1 of *winnerMask* select left or right; bit 2 indicates a tie.
* *txRaceState(raceState newState)* – Requests a state change. In *RACE\_COMPLETE* the finish controller calls this with *RACE\_IDLE* when ready to return to idle and waits for an acknowledgement.



Refer to serialComm.h for additional helpers (e.g. sending reaction times, foul status, etc.). Some enumerations in serialComm.h require minor syntax fixes (missing commas); update these as necessary to compile.



### Communication with Race Manager (BLE)



The Nano 33 BLE includes a Nordic BLE radio that can send race results to a Raspberry Pi. The BLE protocol and characteristic layout are not yet defined. When implemented, race results (*carTimeUs*, *raceTimeUs*, *reactionTimeUs*, *foul*, *carID*, etc.) should be transmitted once the race completes. Consider using BLE notifications to push results to a connected central. Add functions in finishController.cpp (e.g. *txResultsToManager(const RaceResults\&)*) and call them after computing results in *RACE\_RACING* or *RACE\_COMPLETE*.



### Unknowns \& Future Work

* **Confirm wiring and timings** – Verify the mapping of the decimal flags and BCD bits on your custom shield. Adjust the constant definitions and *buildPattern*() logic accordingly. Use an oscilloscope to ensure the latch pulses meet the timing requirements of the **74HC595**, **74HC137** and **MC14543**.
* **Add demultiplexer control** – If the **74HC137** requires a latch enable or chip enable toggle separate from the **74HC595** latch, add additional output pins and drive them in *shiftOut16*(). The **KiCad** schematic suggests ~LE, ~E1 and E2 inputs on the demux that may need to be driven high/low at appropriate times.
* **Integrate RFID** – The *RaceResults* structure reserves a *carID* field for a 4‑byte RFID tag. Extend *serialComm* to send and receive car IDs and populate this field when available.
* **Implement BLE** – Define a BLE GATT service to transmit race results and, if desired, receive configuration commands from the race manager. Ensure that BLE communication does not block timing critical sections.
* **Unify global variables** – Several variables in *globals.h* and *serialComm.h* overlap in purpose (e.g. foul flags). Consolidate these to avoid confusion. Also ensure that all enumerations in *serialComm.h* have trailing commas to avoid compilation errors.



### Recommendations \& Next Steps

1. **Review and test the hardware** using a logic analyser or oscilloscope. Confirm that the sensors trigger correctly, that the minimum race time filter suppresses false triggers, and that the displays show stable digits without flicker.
2. **Simulate races** by manually breaking the beams and sending artificial reaction times via the start controller. Validate that car times are computed correctly (addition for fouls, subtraction otherwise) and that rounding to the nearest millisecond matches your expectations.
3. **Verify the display driver** – Check that the demultiplexer correctly routes each digit and that decimal points illuminate for the proper lane and mode. Adjust the constants if necessary.
4. **Refine serial protocols** – Ensure the start controller sends foul and reaction messages promptly and that the finish controller acknowledges them. Handle error conditions (timeouts, invalid messages) gracefully and update the error enumeration in serialComm.h.
5. **Plan BLE integration** – Sketch out a BLE data format for race results and implement a simple test to send static results to a smartphone or Raspberry Pi. Use this to refine the design before integrating into the main firmware.



By following this design and iteratively verifying each subsystem, you can build a robust and understandable finish controller that will serve as a solid foundation for a more fully featured derby timing system.



## Race Manager System



