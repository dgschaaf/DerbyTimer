# DerbyTimer Architecture Reference

> Human-readable design docs: [docs/startController.md](../docs/startController.md) | [docs/finishController.md](../docs/finishController.md)

---

## System Diagram

```
[Start Controller]  ──── UART 115200 baud ────  [Finish Controller]
 Arduino Nano                                     Arduino Nano 33 BLE
 ATmega328P                                       nRF52840
                                                       │
                                               (future BLE)
                                                       │
                                              [Race Manager - RPi]
                                              (not yet implemented)
```

---

## State Machine

Both controllers share the same `raceState` enum (`globals.h`) and run identical 6×6 allowed-transition tables. The **Start Controller drives** transitions; the Finish Controller follows via `rxTransition()`.

```
IDLE ──[Start]──> STAGING ──[Start]──> COUNTDOWN ──[GO]──> RACING ──[Complete]──> COMPLETE
 ^                   |                                                                  |
 └───────[Mode]──────┘                                               [Start/Display]───┘
 ^
 └── TEST (exists; immediately self-transitions back to IDLE)
```

Two transition paths:
- `selfTransition()` — initiates a coordinated change, requires ACK from peer before committing
- `rxTransition()` — receives a change from peer, commits immediately

---

## Communication Protocol

**File:** `firmware/lib/shared/serialComm.cpp/.h`
**Baud:** 115,200 | **Retries:** 3 | **Timeout:** 50 ms per retry

### Message Types (12 total)

| ID | Name | Direction | Purpose |
|----|------|-----------|---------|
| 0 | MSG_NULL | — | Placeholder/init |
| 1 | MSG_ACK | both | Acknowledge |
| 2 | MSG_NACK | both | Not-acknowledge |
| 3 | MSG_RACE_MODE | SC→FC | Mode change |
| 4 | MSG_RACE_STATE | both | State transition |
| 5 | MSG_RACE_START | SC→FC | Gates dropped, timers start |
| 6 | MSG_ERROR | both | Error reporting |
| 7 | MSG_LEFT_REACT | SC→FC | Left reaction time (µs) |
| 8 | MSG_RIGHT_REACT | SC→FC | Right reaction time (µs) |
| 9 | MSG_FOUL | SC→FC | Foul bitmask (bit0=L, bit1=R) |
| 10 | MSG_WINNER | FC→SC | Winner mask (bit0=L, bit1=R, bit2=tie) |
| 11 | MSG_DISP_ADVANCE | SC→FC | Advance display (Start button press) |

All received values land in the global `SerialRxState rx` struct (extern from `serialComm.h`). Access via `rx.State`, `rx.Mode`, `rx.LeftFoul`, `rx.LeftReactionTime`, `rx.DisplayAdvanceFlag`, etc.

---

## Start Controller Modules

**Board:** Arduino Nano (ATmega328P) | **Path:** `firmware/startController/src/`

| Module | Responsibility |
|--------|---------------|
| `startController.cpp` | State machine, mode machine, race orchestration |
| `buttons.cpp` | Start (A6), Mode (A7), Left (D18), Right (D19) — analog polling + digital debounce |
| `gates.cpp` | Left (D4) / Right (D7) electromagnets + return solenoid (D6, 500 ms window) |
| `lights.cpp` | Christmas Tree dual 6-light array via 74HC595 (D2 data, D3 clk, D5 latch) |

> Note: `rfid.cpp` is listed in some older docs but is **not yet implemented** — not included in current build.

### Race Modes

| Mode | Enum | Button-accessible | Countdown |
|------|------|:-----------------:|-----------|
| Gate Drop | `MODE_GATEDROP` | Yes | 3×500 ms stages |
| Reaction | `MODE_REACTION` | Yes | 3×500 ms stages |
| Pro Tree | `MODE_PRO` | Yes | 1×400 ms stage |
| Dial-In | `MODE_DIALIIN` | **No** (skipped in cycle) | 3×500 ms stages |

---

## Finish Controller Modules

**Board:** Arduino Nano 33 BLE (nRF52840) | **Path:** `firmware/finishController/src/`

| Module | Responsibility |
|--------|---------------|
| `finishController.cpp` | State machine, race result computation, display coordination |
| `sensors.cpp` | SE61 optical sensors — ISR capture, A1 (left) / A0 (right), min 500 ms filter |
| `display.cpp` | 5-digit 7-seg per lane via 74HC238 demux + MC14543B BCD driver, direct GPIO |

### Display Pin Map

| Signal | Pin | Function |
|--------|-----|----------|
| PIN_BCD_MUX_A/B/C | D2/D3/D4 | Digit address (74HC238 A0-A2) |
| PIN_AD0–AD3 | D5–D8 | BCD data (MC14543B) |
| PIN_DECIMAL | D9 | Decimal point |
| PIN_LANE1 | A2 | Left lane enable (active LOW) |
| PIN_LANE2 | A3 | Right lane enable (active LOW) |

---

## Timing Model

- **raceStartUs** — `micros()` captured at GO (countdown) or gate drop
- **raceTimeUs** — ISR-captured elapsed time from `armSensors()` to sensor trigger
- **reactionTimeUs** — `|raceStartUs - carStartUs|` (microseconds, unsigned)
- **carTimeUs** — `raceTimeUs + (foul ? +1 : -1) * reactionTimeUs`
- Minimum race time: 500,000 µs (suppresses false triggers)
- Maximum race time: 10,000,000 µs (auto-completes unfinished lane)
- Display rounds to nearest millisecond; clamped at 99,998 ms

---

## Key Conventions

- Add new states/modes to `globals.h` enums first, then wire into both controllers
- `SerialRxState rx` is the single source of truth for all inter-controller data
- `"future:"` comments in source mark planned but unimplemented features
- State transition logic is symmetric: same `allowedTransition[][]` table in both controllers
