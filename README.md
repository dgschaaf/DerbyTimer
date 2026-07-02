# DerbyTimer

Pinewood derby timing and race track control system.

**Author:** Darren Schaaf

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
**Documentation:** [docs/startController.md](docs/startController.md)

### Capabilities

- **Four race modes** with mode-button cycling and per-mode light patterns
- **Dual starting gates** — electromagnet hold with spring-return solenoid (500 ms activation window)
- **Christmas Tree lights** — dual 6-light arrays (Blue, Yellow×3, Green, Red) via 74HC595 shift register
- **Reaction time measurement** — microsecond precision using `micros()` from gate-open to car release
- **Foul detection** — early release during countdown logged and flagged per lane
- **Four button inputs** — Start, Mode, Left Lane, Right Lane; hardware-debounced via Schmitt triggers on the PCB shield

Module structure, pin assignments, and timing details:
[docs/startController.md](docs/startController.md).

---

## Finish Controller

**Hardware:** Arduino Nano 33 BLE (nRF52840) on a custom PCB shield  
**Documentation:** [docs/finishController.md](docs/finishController.md)

### Capabilities

- **Optical finish sensors** — SE61 sensors with 74HC14 Schmitt inverter for clean edges; ISR-driven with configurable active-high/low polarity
- **Minimum race time filter** — 500 ms suppresses mechanical bounce false triggers
- **Winner determination** — compares rounded car times; ties supported
- **Dual 5-digit seven-segment displays** — driven via 74HC238 decoder and MC14543B BCD driver, direct GPIO
- **Display advance** — Start Controller signals when to cycle from finish time to reaction time view
- **Car time computation** — reaction time subtracted (normal) or added (foul) from finish time; result rounded to nearest millisecond

Module structure, pin assignments, and display driver details:
[docs/finishController.md](docs/finishController.md).

---

## Serial Communication Protocol

Shared library: `firmware/lib/shared/serialComm.cpp/.h`

12 message types over UART at 115,200 baud with ACK/NACK confirmation and
3-retry / 50 ms timeout. Shared enums (`raceState`, `raceMode`,
`countdownState`) and bitmask constants are defined in
`firmware/lib/shared/raceTypes.h`. Full message table and byte-level wire
format: [docs/protocol.md](docs/protocol.md).

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

Layered strategy — run the cheapest layer first, escalate to hardware only
when the layer below passes:

- **L1 Static Analysis** — compile with `--warnings all` + cppcheck
- **L2 Desktop Unit Tests** — `pio test -e native`; pure logic, no hardware
- **L3 Serial Protocol Tests** — two cross-wired Nanos (`firmware/fwTest/`)
  or a laptop UART monitor (`firmware/tools/uart_monitor.py`)
- **L4 RACE_TEST Self-Test** — built-in; hold MODE at power-up
  (codes: [docs/race-test-codes.md](docs/race-test-codes.md))
- **L5 Debug Serial Mode** — compile with `-DDERBY_DEBUG`
- **Hardware bring-up** — standalone sketches in `hardware/hwTest/`

Full layer map, directory guide, and run instructions:
[docs/testing.md](docs/testing.md). Two-board bring-up procedure:
[docs/bench-test-protocol.md](docs/bench-test-protocol.md).

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

Requires `arduino-cli` on your PATH.

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

Step-by-step recipes for adding race modes and other maintenance guidelines:
[docs/startController.md](docs/startController.md) (Maintenance Guidelines
section).

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
