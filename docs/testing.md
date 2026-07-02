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
| `test_compute/` | Car time calculation (reaction subtraction, foul addition, clamping, winner/DNF logic) |
| `test_display/` | Display digit extraction: us-to-ms rounding, 99.998 clamp, 88.888 sentinel |
| `test_serialcomm/` | The real protocol engine (`serialComm.cpp`): RX parsing per message, payload validation, stale-partial flush, TX ACK/retry/timeout/priority/dedup |
| `test_state_machine/` | Allowed-transition table enforcement for all state pairs |
| `test_timing/` | Overflow-safe microsecond elapsed-time arithmetic, foul/reaction edge cases |

**Mocks:** `firmware/test/shared/Arduino.h` is a scriptable mock of the Arduino API: feedable serial RX buffer, captured TX buffer, and controllable `millis()`/`micros()`. It is deliberately named `Arduino.h` so that production sources' `#include <Arduino.h>` resolves to it in the native build -- this is how `test_serialcomm` compiles the real `serialComm.cpp` instead of a copy. It cannot collide with real builds: only the `[env:native]` include path (`-I firmware/test/shared`) sees it; arduino-cli, PlatformIO embedded builds, and the Arduino IDE never look inside `firmware/test/`. (`arduino_mock.h` is a compatibility shim that includes it.)

**To run (pick one):**

1. **VS Code (easiest):** `Terminal > Run Task... > Run L2 Native Tests`
   (defined in `.vscode/tasks.json`).
2. **PlatformIO terminal:** Command Palette (`Ctrl+Shift+P`) >
   `PlatformIO: New Terminal`, then `pio test -e native`. This terminal has
   `pio` on its PATH; a plain PowerShell/CMD terminal does NOT.
3. **Any terminal, full path:**

```sh
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" test -e native   # PowerShell
```

Note: typing `pio test -e native` into the Command Palette itself does
nothing -- the palette runs editor commands, not shell commands. Use one of
the three routes above. Expected result: every test case succeeds; the
current case count and run history live in [test-log.md](test-log.md).

Requires MinGW GCC on Windows: `choco install mingw` (Chocolatey sets the PATH permanently; open a new terminal after installing). PlatformIO VS Code extension also required (it installs `pio.exe` under `%USERPROFILE%\.platformio\penv\`).

---

## L3a Serial Protocol Tests — Details

**Location:** `firmware/fwTest/`

Two sketches run on two separate Arduino Nanos wired cross-connected (D5/D6 SoftwareSerial):

- `derbySerialTester.ino` — interactive test driver; sends messages and checks ACK/NACK/retry behavior
- `derbySerialResponder.ino` — simulates the peer controller; responds to messages

Full wiring diagram, command reference, and test protocol: `firmware/fwTest/derbySerialTester_Documentation.md`

---

## See Also

- **Two-board integration bench protocol: [docs/bench-test-protocol.md](bench-test-protocol.md)** -- staged bring-up checklist, FTDI tap/simulator wiring, symptom table
- Race self-test result codes: [docs/race-test-codes.md](race-test-codes.md)
- Hardware test procedures: [hardware/hwTest/](../hardware/hwTest/)
- Wire protocol byte-level reference: [docs/protocol.md](protocol.md)
