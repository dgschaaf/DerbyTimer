# Derby Track Start Controller System
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
| \*\*startController\*\* | Main state machine and orchestration | `setup()`, `loop()`, state transitions |
| \*\*serialComm\*\* | UART communication protocol | `rxSerial()`, `txRaceState()`, ACK/NACK handling |
| \*\*lights\*\* | LED tree control via shift register | `updateLights()`, `buildLightConfig()`, `startBlink()` |
| \*\*gates\*\* | Electromagnet and solenoid control | `dropGate()`, `returnGates()` |
| \*\*buttons\*\* | Input debouncing and detection | `isStartPressed()`, `isModePressed()` |
| \*\*rfid\*\* | NFC car identification | `readTag()`, `setupRFID()` |
| \*\*globals\*\* | Shared enumerations and constants | Race states, modes, bit masks |

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
| \*\*Serial Baud\*\* | 115,200 | High-speed UART |
| \*\*Reaction Precision\*\* | ±1 µs | Using micros() |
| \*\*Message Timeout\*\* | 50 ms | Balance between responsiveness and reliability |
| \*\*RFID Read Rate\*\* | 2 Hz | 500ms threshold between reads |
| \*\*Countdown Timing\*\* | 400-500 ms | Mode-dependent staging |

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