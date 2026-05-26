# DerbyTimer Serial Protocol Test Harness

## Overview

Two-Nano test rig that verifies the UART wire protocol in isolation — no PCBs or sensors required.

- **Tester Nano** — runs `derbySerialTester.ino`, sends test messages over SoftwareSerial, reports results via USB serial to PC
- **Responder Nano** — runs `derbySerialResponder.ino`, parses inbound bytes, ACKs (or NACKs/silences per mode)

## Wiring

```text
Tester Nano                    Responder Nano
  D5 (TX) ───────────────────> D0 (RX)
  D6 (RX) <─────────────────── D1 (TX)
  GND ──────────────────────── GND
  USB -> PC (serial monitor)   USB -> PC (for flashing only)
```

## Compile and Flash

Both sketches are self-contained — no `--library` flag needed:

```bash
arduino-cli compile --fqbn arduino:avr:nano firmware/fwTest/derbySerialTester.ino
arduino-cli compile --fqbn arduino:avr:nano firmware/fwTest/derbySerialResponder.ino
```

Flash Tester to Nano A. Flash Responder to Nano B. Cross-wire per diagram above.

## Running Tests

Open serial monitor on the Tester's USB port at 115200 baud.

Press `a` to run all tests, or use individual keys:

| Key | Test |
| --- | ---- |
| `a` | Run ALL tests |
| `1` | MSG_RACE_MODE (all 4 modes) |
| `2` | MSG_RACE_STATE (all 6 states) |
| `3` | MSG_RACE_START (priority message) |
| `4` | MSG_LEFT_REACT + MSG_RIGHT_REACT (4-byte payload, positive/negative/zero/INT32_MAX) |
| `5` | MSG_FOUL (foul_left, foul_right, foul_both) |
| `6` | MSG_WINNER (leftWin, rightWin, tie, noResult) |
| `7` | MSG_DISP_ADVANCE (zero payload) |
| `8` | MSG_ERROR — verify ACK received (P2-15 fix) |
| `9` | NACK retry — Responder NACKs first attempt, ACKs second |
| `t` | Timing stress — 50 MSG_RACE_MODE messages; min/max/avg round-trip |
| `e` | Error handling — NACK retry, max retries (TX_FAILED), timeout |
| `r` | Reset Responder mode to ACK_ALL, send IDLE state |
| `s` | Full race sequence: IDLE->STAGING->COUNTDOWN->RACING->COMPLETE->IDLE + WINNER |
| `p` | Print current stats |
| `h` | Help |

## Responder Mode Control

The Tester configures the Responder by sending byte `0xFD` followed by a mode byte.
The Tester's `setRespMode()` calls this automatically before each relevant test.

| Mode | Behavior |
| ---- | -------- |
| `0x00` | ACK all messages (default) |
| `0x01` | NACK next message, then revert to ACK_ALL |
| `0x02` | NACK all messages until explicitly changed |
| `0x03` | No response to next message (silent), then revert to ACK_ALL |

## Test Output Format

```text
[TEST] MSG_RACE_MODE (all 4 modes)
  TX: 0x3 [0]
  RX: 0x1 [3] (842 us)
  PASS: ACK ok
  TX: 0x3 [1]
  RX: 0x1 [3] (810 us)
  PASS: ACK ok

========== TEST SUMMARY ==========
Total: 24  Pass: 24  Fail: 0  Timeout: 0
Round-trip us  min: 803  max: 1201  avg: 921
==================================
```

## Troubleshooting

| Symptom | Likely Cause | Fix |
| ------- | ------------ | --- |
| All tests timeout | TX/RX wires swapped | Swap D5 and D6 on Tester side |
| All tests timeout | Baud mismatch | Verify both boards at 115200 |
| Intermittent timeouts | SoftwareSerial jitter at high baud | See P2-36 for baud rate evaluation |
| NACK instead of ACK | Payload byte count mismatch | Check MsgID enum order in both sketches matches serialComm.h |
| Wrong ACK ID | MSG_ERROR not ACKed | Verify P2-15 fix is in serialComm.cpp |

## Protocol Independence

Both sketches define local copies of the protocol constants (MsgID enum, bitmask values) rather
than including `serialComm.h`. This keeps the harness independent of the production library —
a bug in `serialComm.cpp` does not mask itself in the test output.

If the production protocol changes (new message ID, different payload size), update both
`derbySerialTester.ino` and `derbySerialResponder.ino` to match `serialComm.h`.

## Response Time Guidelines

| Category | Typical | Concerning |
| -------- | ------- | ---------- |
| ACK for 1-byte msg | 500-1500 us | > 5 ms |
| ACK for 4-byte msg | 1000-2500 us | > 10 ms |
| Race start to ACK | < 2 ms | > 5 ms |
