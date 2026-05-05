# CLAUDE.md

DerbyTimer is a hobbyist pinewood derby timing and race control system built on two cooperating Arduino microcontrollers connected by UART.

> Prefer conceptual explanations alongside code changes — Darren is actively learning embedded development. See [.claude/collaboration.md](.claude/collaboration.md).

---

## Quick Reference

### Build & Upload

```bash
# Start Controller (Arduino Nano ATmega328P)
arduino-cli compile --fqbn arduino:avr:nano firmware/startController/startController.ino
arduino-cli upload  --fqbn arduino:avr:nano -p <PORT> firmware/startController/startController.ino

# Finish Controller (Arduino Nano 33 BLE, nRF52840)
arduino-cli compile --fqbn arduino:mbed_nano:nano33ble firmware/finishController/finishController.ino
arduino-cli upload  --fqbn arduino:mbed_nano:nano33ble -p <PORT> firmware/finishController/finishController.ino
```

Serial monitor: **115,200 baud** (both controllers). Arduino IDE is primary; `arduino-cli` is optional.

---

## Repository Structure

```text
firmware/
  startController/      # Arduino Nano (ATmega328P)
  finishController/     # Arduino Nano 33 BLE (nRF52840)
  lib/shared/           # globals.h + serialComm — shared by both controllers
  swTest/               # Serial protocol test sketches
  raceManager/          # Future RPi BLE race manager (incomplete stub)
hardware/
  startBoard/           # KiCad schematic + PCB for start controller shield
  finishBoard/          # KiCad schematic + PCB for finish controller shield
  hwTest/               # Hardware functional test sketches + protocols
docs/                   # Human-readable design docs (startController, finishController)
.claude/                # Claude-specific reference files (see index below)
```

---

## Reference Index

| File | Purpose |
| ---- | ------- |
| [.claude/architecture.md](.claude/architecture.md) | System diagram, state machine, serial protocol, module tables, timing model, key conventions |
| [.claude/project-status.md](.claude/project-status.md) | Living P0/P1/P2 issue tracker — update this when items are resolved or discovered |
| [.claude/collaboration.md](.claude/collaboration.md) | How Darren likes to work, what he's learning, past mistakes to avoid |
| [.claude/conventions.md](.claude/conventions.md) | Code conventions, file encoding, git workflow, doc conventions |
| [.claude/notes/open_questions.md](.claude/notes/open_questions.md) | Deferred investigation items requiring a decision |
| [docs/startController.md](docs/startController.md) | Start controller design doc (human-facing, kept current) |
| [docs/finishController.md](docs/finishController.md) | Finish controller design doc (human-facing, kept current) |

---

## Testing

**Protocol tests** — upload `firmware/swTest/derbySerialTester.ino` + `derbySerialResponder.ino` to the two controllers. Exercises all 12 message types; reports pass/fail over serial. See `firmware/swTest/derbySerialTester_Documentation.md`.

**Hardware functional tests** — `hardware/hwTest/startController_hw_functional_test.ino` and `finishController_hw_functional_test.ino`. Corresponding `.md` files document expected behavior per peripheral.
