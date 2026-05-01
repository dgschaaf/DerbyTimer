# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DerbyTimer is a hobbyist pinewood derby timing and race control system. It uses two cooperating microcontrollers — a Start Controller and a Finish Controller — connected by a UART serial link.

## Build & Upload

This project uses **Arduino IDE** (not PlatformIO). There is no automated build command; sketches are compiled and uploaded from the Arduino IDE or `arduino-cli`.

To build with `arduino-cli` (if installed):
```bash
# Start Controller (Arduino Nano ATmega328P)
arduino-cli compile --fqbn arduino:avr:nano firmware/startController/startController.ino
arduino-cli upload  --fqbn arduino:avr:nano -p <PORT> firmware/startController/startController.ino

# Finish Controller (Arduino Nano 33 BLE, nRF52840)
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble firmware/finishController/finishController.ino
arduino-cli upload  --fqbn arduino:mbed_nano:nano33ble -p <PORT> firmware/finishController/finishController.ino
```

Serial monitor baud rate: **115,200** (both controllers).

## Repository Structure

```
firmware/
  startController/    # Arduino Nano (ATmega328P) firmware
    startController.ino
    src/              # State machine + module implementations
  finishController/   # Arduino Nano 33 BLE (nRF52840) firmware
    finishController.ino
    src/
  lib/shared/         # Code shared by both controllers
    globals.h         # Enums (raceState, raceMode, countdownState) + bitmasks
    serialComm.cpp/h  # UART protocol implementation
  swTest/             # Software/protocol test sketches
  raceManager/        # Future Raspberry Pi BLE race manager (incomplete)
hardware/
  startBoard/         # KiCad schematic + PCB for start controller shield
  finishBoard/        # KiCad schematic + PCB for finish controller shield
  hwTest/             # Hardware functional test sketches + test protocols (.md)
docs/                 # Detailed design documents for each subsystem
```

## Architecture

### System Design

```
[Start Controller]  ──── UART 115200 baud ────  [Finish Controller]
 Arduino Nano                                     Arduino Nano 33 BLE
 ATmega328P                                       nRF52840
                                                       │
                                               (future BLE)
                                                       │
                                              [Race Manager - RPi]
```

### State Machine

Both controllers share the same `raceState` enum and stay in sync via serial messages:

```
IDLE → STAGING → COUNTDOWN → RACING → COMPLETE → (back to IDLE)
```

The **Start Controller** drives state transitions; the Finish Controller follows. State is kept in `currentState` / `targetState` globals and sent as `MSG_STATE` messages.

### Communication Protocol (`firmware/lib/shared/serialComm.cpp`)

14 message types over UART. Key reliability features:
- ACK/NACK confirmation on every message
- 3-retry limit with 50 ms timeout per retry
- The finish controller calls `rxSerial()` frequently in the main loop to parse incoming messages and update shared globals (`rxLeftReactionTime`, `rxRightReactionTime`, `rxLeftFoul`, `rxRightFoul`, `rxDisplayAdvanceFlag`)

### Start Controller Modules (`firmware/startController/src/`)

| Module | Responsibility |
|--------|---------------|
| `startController.cpp` | Top-level state machine, orchestrates all modules |
| `buttons.cpp` | 4-button debounced input (Start, Mode, Left Lane, Right Lane) |
| `gates.cpp` | Electromagnet hold + spring-return solenoid (500 ms window) |
| `lights.cpp` | "Christmas Tree" dual 6-light array via 74HC595 shift register |
| `rfid.cpp` | Dual RC522 RFID readers for car identification |

### Finish Controller Modules (`firmware/finishController/src/`)

| Module | Responsibility |
|--------|---------------|
| `finishController.cpp` | State machine + race result computation |
| `sensors.cpp` | SE61 optical finish sensors with ISR + min/max time filtering |
| `display.cpp` | Two 5-digit seven-segment displays via chained 74HC595 + 74HC137 demux + MC14543B BCD driver |

### Timing Model

- **Reaction time**: measured in microseconds (`micros()`) from gate open to car detection
- **Race time**: ISR-captured microsecond timestamp from `armSensors()` call
- **Car time**: `raceTime - reactionTime` (normal) or `raceTime + reactionTime` (foul); rounded to nearest millisecond
- Minimum race time filter: 500,000 µs (0.5 s) suppresses false triggers

### Key Design Conventions

- Shared enums live in `globals.h` — extend `raceMode` or `raceState` there when adding modes
- Both controllers use `extern` globals for serial-received values (appropriate for single-threaded embedded targets)
- "future:" comments in source mark planned features (BLE integration, car ID transmission)
- `maxRaceTimeUs` (10 s) auto-completes a lane if sensor never triggers

## Testing

**Protocol tests** (`firmware/swTest/`): Upload `derbySerialTester.ino` to one controller and `derbySerialResponder.ino` to the other. The tester exercises all 14 message types and reports pass/fail counts over serial. See `derbySerialTester_Documentation.md` for the test protocol.

**Hardware functional tests** (`hardware/hwTest/`): `startController_hw_functional_test.ino` and `finishController_hw_functional_test.ino` cycle through hardware peripherals. Corresponding `.md` files document expected behavior and connector layout for each test.

## Known Issues / In-Progress

- BLE communication to race manager is not yet implemented
- Some enumerations in `serialComm.h` have missing trailing commas — fix before compiling
- RFID car ID transmission over serial is reserved in `RaceResults` but not yet wired up
- Finish controller display wiring constants (`DECIMAL_LEFT_TIME` etc.) may need adjustment to match actual shield PCB routing
