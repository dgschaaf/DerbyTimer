# Test Suite Overview

## Philosophy

DerbyTimer uses a layered test strategy: offline tests (L1–L2) catch logic errors without any hardware; integration tests (L3) validate the serial protocol between real Nanos; built-in firmware tests (L4–L5) run on the deployed hardware itself. Hardware bring-up tests live separately under `hardware/` and are used once when assembling a new PCB.

Run the cheapest layer first. Only escalate to hardware when the layer below passes.

---

## Test Layers at a Glance

| Layer | What it tests | Where to find it | How to run |
|-------|--------------|-----------------|------------|
| L1 — Static Analysis | Compiler warnings, undefined behavior, type mismatches | Build flags (`--warnings all`) + cppcheck extension | Compile either controller in VS Code (`Ctrl+Shift+B`) |
| L2 — Desktop Unit Tests | Pure C++ logic (bitmasks, timing math, state machine transitions) | `firmware/test/native/` | `pio test -e native` |
| L3a — Serial Protocol Tests | Full UART message exchange between two Nanos | `firmware/fwTest/` | Upload sketches to two Nanos; see `derbySerialTester_Documentation.md` |
| L3b — UART Monitor | Protocol inspection and scripted injection from a laptop | `firmware/tools/uart_monitor.py` | `python firmware/tools/uart_monitor.py <COMx>` |
| L4 — RACE_TEST Self-Test | All peripherals on both controllers (lights, gates, sensors, display, comms) | Built into firmware | Hold MODE button at power-up; read results from lights/display |
| L5 — Debug Serial Mode | State transitions, TX/RX events, sensor fires logged over serial | `firmware/lib/shared/debug.h` | Compile with `-DDERBY_DEBUG`; open serial monitor at 115200 |
| Hardware Tests | Board-level peripheral bring-up after PCB assembly | `hardware/hwTest/` | Upload standalone `.ino` sketch; follow `.md` checklist |

---

## L2 Desktop Unit Tests — Details

**Location:** `firmware/test/native/`

| Suite | What it covers |
|-------|---------------|
| `test_bitmasks/` | Bitfield encoding/decoding for foul, winner, and mode flags |
| `test_compute/` | Car time calculation (reaction subtraction, foul addition, rounding) |
| `test_state_machine/` | Allowed-transition table enforcement for all state pairs |
| `test_timing/` | Overflow-safe microsecond elapsed-time arithmetic |

**Mocks:** `firmware/test/shared/arduino_mock.h` provides stub implementations of `millis()`, `micros()`, and `Serial` so the tests compile and run on a Windows/Mac desktop without any Arduino attached.

**To run:**

```sh
pio test -e native
```

Requires the PlatformIO VS Code extension and `pio pkg install` (see P2-35 in `.claude/project-status.md` if the native platform is not yet installed).

---

## L3a Serial Protocol Tests — Details

**Location:** `firmware/fwTest/`

Two sketches run on two separate Arduino Nanos wired cross-connected (D5/D6 SoftwareSerial):

- `derbySerialTester.ino` — interactive test driver; sends messages and checks ACK/NACK/retry behavior
- `derbySerialResponder.ino` — simulates the peer controller; responds to messages

Full wiring diagram, command reference, and test protocol: `firmware/fwTest/derbySerialTester_Documentation.md`

---

## See Also

- Race self-test result codes: [docs/race-test-codes.md](race-test-codes.md)
- Hardware test procedures: [hardware/hwTest/](../hardware/hwTest/)
- Serial protocol reference: [.claude/architecture.md](../.claude/architecture.md)
