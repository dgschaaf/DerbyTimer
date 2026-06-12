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
| Dial-In | BLE-activated only (Race Manager required); not accessible via the mode button. Pressing Mode while active returns to Gate Drop |

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
- **Four button inputs** — Start, Mode, Left Lane, Right Lane; hardware-debounced via Schmitt triggers on the PCB shield

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

Shared enums (`raceState`, `raceMode`, `countdownState`) and bitmask constants are defined in `firmware/lib/shared/raceTypes.h`. Byte-level wire format reference: [docs/protocol.md](docs/protocol.md).

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

> Full layer map, directory guide, and run instructions: [docs/testing.md](docs/testing.md)

### L1 — Static Analysis

`--warnings all` is included in the VS Code build tasks. `cppcheck` is in `.vscode/extensions.json` recommendations. Catches uninitialized variables, missing `switch` defaults, and signed/unsigned mismatches at compile time.

### L2 — Desktop Unit Tests (`test/native/`)

PlatformIO native environment (`pio test -e native`). Tests and stubs live in `firmware/test/`. Activate by installing the PlatformIO VS Code extension and running `pio pkg install` (see P2-35 in `.claude/project-status.md`).

### L3a — Serial Protocol Tests (`firmware/fwTest/`)

Upload `derbySerialTester.ino` to one Nano and `derbySerialResponder.ino` to a second Nano wired cross-connected (D5/D6 SoftwareSerial). Interactive serial commands test all message types, ACK/NACK retry, timeout, priority queue, and full race sequence simulation. See `derbySerialTester_Documentation.md` for wiring and test protocol.

### L3b — Laptop UART Monitor (`tools/uart_monitor.py`)

`python firmware/tools/uart_monitor.py <COMx>` — four modes: passive monitor (log all messages), SC simulator (scripted state walk), FC simulator (auto-ACK + send winner on keypress), protocol injector (interactive menu). Requires `pip install pyserial`.

### L4 — RACE_TEST Self-Test (built-in)

Hold the MODE button on the Start Controller at power-up to enter self-test mode. Both controllers run independent test sequences (see `docs/race-test-codes.md`):

- **SC**: FC communication ping, light chase (all 8 LEDs), gate cycle (return + drop), interactive button verification
- **FC**: Display segment test (88.888 countdown), sensor beam-break verification, SC communication check
- Results shown on SC lights and FC displays with failure codes for any failed phase.

### L5 — Debug Serial Mode (`firmware/lib/shared/debug.h`)

Compile with `-DDERBY_DEBUG` to enable structured serial log output on any state transition, TX/RX event, sensor fire, or foul detection — without affecting timing. See the debug task in `.vscode/tasks.json`.

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

### Option 3 — Arduino IDE (not recommended)

The IDE does not know about `firmware/lib/shared`, so opening the `.ino`
directly fails with `serialComm.h: No such file or directory`. To use the
IDE anyway: copy the `firmware/lib/shared/` folder into your sketchbook
`libraries/` folder (e.g. `Documents/Arduino/libraries/shared/`) and re-copy
it after every shared-library change. Boards: Start Controller = Arduino
Nano (ATmega328P); Finish Controller = Arduino Nano 33 BLE. Options 1 and 2
avoid all of this — prefer them.

### Serial monitoring

Debug builds (`-DDERBY_DEBUG`) log at **115,200** baud. The Finish
Controller's debug output (USB) coexists with live racing — the wire
protocol runs on Serial1 (D0/D1). The Start Controller's single UART is
shared between USB and the wire protocol: never flash or monitor the SC
over USB with the comm cable connected. Full bench procedure:
[docs/bench-test-protocol.md](docs/bench-test-protocol.md).

---

## Extending the Design

### Adding a Race Mode

1. Extend `raceMode` enum in `raceTypes.h`
2. Add case to the mode-button handler in `startController.cpp`
3. Define the corresponding light pattern in `lights.cpp`
4. Implement countdown behavior if it differs from the standard sequence

---

## Known Limitations

The following are intentional v1.0 scope boundaries, not bugs:

- **BLE / Race Manager** — The Finish Controller's Nordic radio is unused. BLE integration is planned for the `feature-raceManager` branch. Placeholder call sites are marked `// future:` in source.
- **RFID car identification** — Not implemented in v1.0. Reserved for the `feature-rfid` branch.
- **Dial-In mode** — Implemented in firmware but only activatable via BLE from a Race Manager. Cannot be selected via the mode button in standalone operation.
- **Growth feature branches** — `feature-rfid` and `feature-raceManager` are design and early development branches. They are not merged to main, are not guaranteed to compile, and are not part of v1.0 scope.

---

## Growth Features (In-Progress Branches)

The following capabilities are under active development on separate branches and are not part of the v1.0 release:

### RFID Car Identification (`feature-rfid`)

Dual RC522 RFID readers on the Start Controller shield identify individual cars at staging. Car IDs are transmitted to the Finish Controller and stored in `RaceResults`. This enables per-car timing history and automatic lane assignment.

### Race Manager (`feature-raceManager`)

A Raspberry Pi-based race management system that receives results from the Finish Controller via BLE (Nordic radio on the Nano 33 BLE). The Race Manager software lives in `software/raceManager/` and uses Python and SQLite for race data storage, bracket management, and results reporting.
