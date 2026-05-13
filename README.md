# DerbyTimer

Pinewood derby timing and race track control system.

**Author:** Darren Schaaf  
**Version:** 1.0 (Initial Release)

---

## System Overview

DerbyTimer is a two-controller embedded system that manages the complete lifecycle of a pinewood derby race — from staging and countdown through finish line detection and result display.

```
[Start Controller]  ──── UART 115,200 baud ────  [Finish Controller]
 Arduino Nano (ATmega328P)                         Arduino Nano 33 BLE (nRF52840)
 Custom PCB shield                                 Custom PCB shield
```

The **Start Controller** manages gates, lights, buttons, and race sequencing. The **Finish Controller** measures finish times, drives result displays, and determines the winner. Both run independent state machines that stay synchronized over a serial protocol.

---

## Race Modes

| Mode | Behavior |
|------|----------|
| Gate Drop | Both gates release simultaneously; no reaction time |
| Reaction Time | Cars held at gate; release time measured per lane |
| Pro Tree | Accelerated countdown sequence (400 ms steps) |
| Dial-In | Reserved for future use |

## Race State Flow

```
IDLE → STAGING → COUNTDOWN → RACING → COMPLETE → IDLE
```

The Start Controller drives state transitions and notifies the Finish Controller via serial. Mode changes and display advances can also be triggered from the Start Controller.

---

## Start Controller

**Hardware:** Arduino Nano (ATmega328P) on a custom PCB shield  
**Documentation:** `docs/startController.md`

### Capabilities

- **Four race modes** with mode-button cycling and per-mode light patterns
- **Dual starting gates** — electromagnet hold with spring-return solenoid (500 ms activation window)
- **Christmas Tree lights** — dual 6-light arrays (Blue, Yellow×3, Green, Red) via 74HC595 shift register
- **Reaction time measurement** — microsecond precision using `micros()` from gate-open to car release
- **Foul detection** — early release during countdown logged and flagged per lane
- **Four button inputs** — Start, Mode, Left Lane, Right Lane with debouncing

### Module Structure

| Module | Responsibility |
|--------|---------------|
| `startController.cpp` | Top-level state machine and orchestration |
| `buttons.cpp` | Debounced input for 4 buttons |
| `gates.cpp` | Electromagnet hold + solenoid return control |
| `lights.cpp` | Christmas Tree LED array via shift register |

### Pin Notes

See schematic for full mapping. D18/D19 are used for buttons (corrected from original D11/D12 to avoid SPI conflicts).

### Timing

| Metric | Value |
|--------|-------|
| Reaction precision | ±1 µs |
| Gate return window | 500 ms |
| Countdown step (standard) | 500 ms |
| Countdown step (Pro Tree) | 400 ms |

---

## Finish Controller

**Hardware:** Arduino Nano 33 BLE (nRF52840) on a custom PCB shield  
**Documentation:** `docs/finishController.md`

### Capabilities

- **Optical finish sensors** — SE61 sensors with 74HC14 Schmitt inverter for clean edges; ISR-driven with configurable active-high/low polarity
- **Minimum race time filter** — 500 ms suppresses mechanical bounce false triggers
- **Winner determination** — compares rounded car times; ties supported
- **Dual 5-digit seven-segment displays** — driven via 74HC238 decoder and MC14543B BCD driver, direct GPIO
- **Display advance** — Start Controller signals when to cycle from finish time to reaction time view
- **Car time computation** — reaction time subtracted (normal) or added (foul) from finish time; result rounded to nearest millisecond

### Module Structure

| Module | Responsibility |
|--------|---------------|
| `finishController.cpp` | State machine, race logic, winner determination |
| `sensors.cpp` | ISR-based optical sensor handling with time filtering |
| `display.cpp` | Direct GPIO display driver (74HC238 + MC14543B BCD) |

---

## Serial Communication Protocol

Shared library: `firmware/lib/shared/serialComm.cpp/.h`

**12 message types** over UART at 115,200 baud with ACK/NACK confirmation and 3-retry / 50 ms timeout.

| Direction | Messages |
|-----------|----------|
| Start → Finish | `MSG_RACE_MODE`, `MSG_RACE_STATE`, `MSG_RACE_START`, `MSG_LEFT_REACT`, `MSG_RIGHT_REACT`, `MSG_FOUL`, `MSG_DISP_ADVANCE`, `MSG_ERROR` |
| Finish → Start | `MSG_WINNER`, `MSG_RACE_STATE` |
| Both | `MSG_ACK`, `MSG_NACK` |

Shared enums (`raceState`, `raceMode`, `countdownState`) and bitmask constants are defined in `firmware/lib/shared/raceTypes.h`.

---

## Hardware Design

KiCad schematics and PCB layouts are in `hardware/startBoard/` and `hardware/finishBoard/`.

### Power

| Rail | Purpose |
|------|---------|
| 12V | Electromagnets and Christmas Tree lights |
| 5V | Logic (externally regulated) |
| 3.3V | Reserved (externally regulated) |
| ~1A peak | During gate return solenoid activation |

---

## Testing

### Software Protocol Tests (`firmware/swTest/`)

Upload `derbySerialTester.ino` to one controller and `derbySerialResponder.ino` to the other. Interactive serial commands test all 13 message types, timing, error handling, and full race sequence simulation. See `derbySerialTester_Documentation.md` for the test protocol.

### Hardware Tests (`hardware/hwTest/`)

- `startController_hw_functional_test.ino` — cycles through all start controller peripherals
- `finishController_hw_functional_test.ino` — cycles through all finish controller peripherals
- Corresponding `.md` files document connector layout and expected behavior for each test

---

## Deployment

### Required Libraries

- Standard Arduino libraries (AVR core)
- MBED OS (Nano 33 BLE — for interrupt support)

### Build

### Option 1 — VS Code (recommended)

A `.vscode/tasks.json` is included. With the project open in VS Code:

- **Ctrl+Shift+B** — compiles the Start Controller (default build task)
- **Terminal > Run Task... > Compile Finish Controller** — compiles the Finish Controller

Requires `arduino-cli` to be on your PATH (or at `C:\Users\Darren\bin\arduino-cli.exe`).

### Option 2 — arduino-cli (command line)

```sh
arduino-cli compile --fqbn arduino:avr:nano --library firmware/lib/shared firmware/startController/startController.ino
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble --library firmware/lib/shared firmware/finishController/finishController.ino
```

### Option 3 — Arduino IDE 1.8+

Open the `.ino` sketch file and select the correct board before compiling:

| Controller | Board |
|------------|-------|
| Start Controller | Arduino Nano (ATmega328P) |
| Finish Controller | Arduino Nano 33 BLE |

Serial monitor baud rate: **115,200** for both controllers.

---

## Extending the Design

### Adding a Race Mode

1. Extend `raceMode` enum in `raceTypes.h`
2. Add case to the mode-button handler in `startController.cpp`
3. Define the corresponding light pattern in `lights.cpp`
4. Implement countdown behavior if it differs from the standard sequence

---

## Growth Features (In-Progress Branches)

The following capabilities are under active development on separate branches and are not part of the v1.0 release:

### RFID Car Identification (`feature-rfid`)

Dual RC522 RFID readers on the Start Controller shield identify individual cars at staging. Car IDs are transmitted to the Finish Controller and stored in `RaceResults`. This enables per-car timing history and automatic lane assignment.

### Race Manager (`feature-raceManager`)

A Raspberry Pi-based race management system that receives results from the Finish Controller via BLE (Nordic radio on the Nano 33 BLE). The Race Manager software lives in `software/raceManager/` and uses Python and SQLite for race data storage, bracket management, and results reporting.
