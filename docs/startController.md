# Derby Track Start Controller System

> As-built design reference. The firmware in `firmware/startController/src/` is
> authoritative; this document explains the design and the reasoning behind it.

### System Overview

The Start Controller manages the beginning of pinewood derby races, controlling starting gates and countdown lights. It operates as an embedded system on an Arduino Nano (ATmega328P) with a custom PCB shield, communicating with a separate Finish Controller via serial UART.

### Core Capabilities

#### Race Management

* **Four Race Modes**: Gate Drop, Reaction Time, Pro Tree, and Dial-In. The first three cycle via the mode button. Dial-In is only activated by a BLE command from the Race Manager -- it requires historical run data to compute handicaps, which is beyond the scope of the standalone controllers.
* **Six Race States**: Idle -> Staging -> Countdown -> Racing -> Complete -> (back to Idle); Test state (RACE_TEST) runs a 4-phase self-test: FC communication ping, light chase, gate cycle, and interactive button verification
* **Automated Sequencing**: Handles complete race lifecycle with state-driven logic

#### Hardware Control
* **Dual Starting Gates**: Electromagnet-held gates with spring-loaded drop mechanism
* **Return Solenoid**: Automated gate reset system with 500 ms activation window
* **Christmas Tree Lights**: Dual 6-light arrays (Blue, Yellowx3, Green, Red) via 74HC595 shift register

#### User Interface

* **Four Button Inputs**: Start (A6), Mode (A7), Left Lane (D18), Right Lane (D19) -- all hardware-debounced via Schmitt triggers on the PCB shield; no software debounce required
* **Visual Feedback**: LED patterns indicate mode, state, and race status
* **Foul Detection**: Red light indication for false starts in reaction modes

### Technical Architecture

#### Module Structure
| Module | Purpose | Key Functions |
|--------|---------|---------------|
| **startController** | Main state machine and orchestration | `startControllerSetup()`, `startControllerLoop()`, state transitions |
| **serialComm** | UART communication protocol | `rxSerial()`, `txRaceState()`, ACK/NACK handling |
| **lights** | LED tree control via 74HC595 shift register | `updateLights()`, `buildLightConfig()`, `startBlink()`, `updateBlink()` |
| **gates** | Electromagnet and solenoid control | `dropGate()`, `returnGates()` |
| **buttons** | Input debouncing and detection | `isStartPressed()`, `isModePressed()`, `isLeftPressed()`, `isRightPressed()` |
| **globals** | Shared enumerations and constants | Race states, modes, bitmasks |

#### Communication Protocol

##### Message Types (12 total):
* State synchronization (MSG_RACE_MODE, MSG_RACE_STATE)
* Race events (MSG_RACE_START, MSG_LEFT_REACT, MSG_RIGHT_REACT, MSG_FOUL, MSG_WINNER, MSG_DISP_ADVANCE)
* Control flow (MSG_ACK, MSG_NACK, MSG_ERROR)
* Null placeholder (MSG_NULL)

##### Reliability Features:
* ACK/NACK confirmation system
* 3-retry limit with 50 ms timeout per retry
* State verification before transitions

#### State Machine Design
```
IDLE ──[Start]──> STAGING ──[Start]──> COUNTDOWN ──[GO]──> RACING ──[Complete]──> COMPLETE
 ^                   |                                                                 |
 └───────[Mode]──────┘                                              [Start/Display]───┘
 ^
 └──── TEST (4-phase self-test; hold MODE at power-up to enter)
```

##### State Characteristics:
* Entry actions (reset, initialization)
* Continuous actions (monitoring, updates)
* Exit actions (cleanup, transmission)
* Guarded transitions via allowed-transition table (same 6x6 table in both controllers)

#### Mode Machine Design

Modes cycle via the Mode button (IDLE state only): Gate Drop -> Reaction -> Pro -> Gate Drop. Each transition blinks the corresponding yellow light 3x. Dial-In is the fourth mode but is not reachable through the button -- it is only activated by the Race Manager via BLE. Once active, pressing the mode button returns to Gate Drop.

| Mode | Light Pattern | Countdown Delay | How Entered |
|------|---------------|-----------------|-------------|
| Gate Drop | Y1 | 500 ms per stage | Mode button |
| Reaction | Y2 | 500 ms per stage | Mode button |
| Pro Tree | Y3 | 400 ms per stage | Mode button |
| Dial-In | GO | 500 ms per stage | BLE from Race Manager only |

### Key Design Decisions

#### 1. Modular Architecture
Each hardware subsystem has its own module with clear interfaces, enabling independent testing and modification.

#### 2. Pragmatic Global State
Uses a `SerialRxState rx` struct (extern from serialComm) for inter-module state visibility, appropriate for embedded systems. Local state and timing data are kept in file-local structs inside each controller source file.

#### 3. Hybrid Programming Style
Combines procedural functions with data structs (`stateMachine`, `modeSelect`, `CountDownCtx`), avoiding over-engineering while maintaining structure.

#### 4. Timing Precision
* Microsecond precision (`micros()`) for reaction time measurement and gate-trigger timestamps
* Millisecond precision (`millis()`) for countdown stage delays and UI timeouts

#### 5. Safety First
* Hardware timeouts on all actuators
* Foul detection in reaction modes (early gate trigger records foul flag and drops gate immediately)
* State transition gating (selfTransition requires ACK before committing)

### Performance Characteristics

| Metric | Value | Notes |
|--------|-------|-------|
| **Serial Baud** | 115,200 | High-speed UART |
| **Reaction Precision** | +/-1 us | Using micros() |
| **Message Timeout** | 50 ms | Per-retry; 3 retries max |
| **Countdown Stage Delay** | 400 ms (Pro) / 500 ms (others) | Between each yellow light |

### Countdown Sequence Detail

On entry to RACE_COUNTDOWN the countdown state machine begins:

* **Gate Drop / Reaction / Dial-In**: CD_STAGED -> CD_Y3 -> CD_Y2 -> CD_Y1 -> CD_GO (500 ms per stage)
* **Pro Tree**: CD_STAGED -> CD_Y1 -> CD_GO (400 ms, one stage -- single yellow flash)

At CD_GO, gates drop (Gate Drop mode) or stay armed (Reaction/Pro modes), `raceStartUs` is captured, and MSG_RACE_START is sent to the finish controller. The state machine transitions to RACE_RACING.

### Reaction Time & Foul Handling

In Reaction/Pro/Dial-In modes, the Left and Right Lane buttons act as car-release triggers:

* **During COUNTDOWN**: A button press before CD_GO is an early start -> gate drops, foul flag set, `carStartUs` recorded.
* **During RACING**: A button press after GO -> gate drops, `carStartUs` recorded, reaction time calculated.

Reaction time = `|raceStartUs - carStartUs|` (absolute difference; stored as unsigned microseconds). A foul reaction time is *added* to race time at the finish controller; a clean reaction time is *subtracted*.

Pending messages (foul status, left reaction, right reaction) are sent one at a time over serial before the state machine accepts a RACE_COMPLETE transition from the finish controller.

### Deployment Notes

#### Hardware Requirements
* Arduino Nano (ATmega328P)
* Custom PCB shield (see KiCad schematics in hardware/startBoard/)
* External 5V regulator for electromagnets and logic
* 74HC595 shift register for lights (D2 data, D3 clock, D5 latch)
* Gate pins: D4 (left electromagnet), D7 (right electromagnet), D6 (return solenoid)

#### Pin Assignments
| Function | Pin |
|----------|-----|
| Start button | A6 (analog, active <=512) |
| Mode button | A7 (analog, active <=512) |
| Left Lane button | D18 (digital, active LOW, external pull-up) |
| Right Lane button | D19 (digital, active LOW, external pull-up) |
| Lights shift-register data | D2 |
| Lights shift-register clock | D3 |
| Lights shift-register latch | D5 |
| Left gate electromagnet | D4 |
| Right gate electromagnet | D7 |
| Return solenoid | D6 |

#### Power Considerations
* 12V supply for electromagnets
* 5V logic with external regulation
* ~1A peak current during gate return

### Maintenance Guidelines

#### Adding Race Modes
1. Extend `raceMode` enum in `raceTypes.h`
2. If the mode should be reachable via the mode button, add it to `nextMode()` in `modeMachine` (startController.cpp). Modes only reachable via BLE (like Dial-In) are intentionally omitted from `nextMode()` but still need the `DIALIIN -> GATEDROP` exit case so the operator can leave the mode via button.
3. Define light pattern in both `selfTransition()` and `rxTransition()` of `modeMachine`
4. Add countdown delay case in `CountDownCtx::tick()` if different from 500 ms

#### Debugging
* Serial output at 115,200 baud
* LED patterns indicate mode and state
* `"future:"` comments in source mark planned features
* State visibility through `stm.current` and `md.current` locals

#### Testing
See [testing.md](testing.md) for the layered test strategy (static analysis
through on-hardware self-test) and
[bench-test-protocol.md](bench-test-protocol.md) for the two-board bring-up
procedure.

### Summary
The Start Controller implements a state-driven embedded system with modular hardware abstraction, a coordinated serial protocol, and microsecond-precision reaction timing. Its clear state and mode machines make it straightforward to extend with new modes or features while the ACK/NACK serial protocol ensures the finish controller stays in sync.
