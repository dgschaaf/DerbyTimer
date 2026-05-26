# Finish Controller Design Notes

This document summarizes the design of the finish controller firmware for a hobbyist Pinewood‑Derby timing system. The **finish controller** complements a separate start controller and reports race results to a future **race manager** (a Raspberry Pi via BLE). It measures finish times, combines them with start reaction times, determines the winner, and drives large seven‑segment displays.

---

### Hardware Overview

* **Controller** – An **Arduino Nano 33 BLE** (nRF52840) runs the finish controller firmware. It communicates with the **start controller** over a UART defined in *serialComm* and will later talk BLE to the race manager.
* **Finish sensors** – Each lane has an **SE61** optical sensor with an external pull‑up resistor. The signals pass through a **74HC14** Schmitt inverter on the shield to provide clean edges. A configurable *activeHigh* flag in the sensors module selects whether the interrupt triggers on a rising or falling edge. A minimum race time (*minRaceTimeUs* = 500,000 µs) filters out spurious triggers during the first 0.5 s; *maxRaceTimeUs* = 10,000,000 µs auto‑completes a lane after 10 s.
* **Display driver** – Each lane's 5‑digit seven‑segment display is driven directly via GPIO rather than through shift registers. The BCD digit value is placed on four output lines (AD0–AD3, pins D5–D8) connected to **MC14543B** BCD‑to‑7‑segment drivers. The digit position is selected by three address lines (A0–A2, pins D2–D4) connected to a **74HC238** 3‑to‑8 demultiplexer. A single decimal‑point line (pin D9) is toggled per digit. Lane selection uses dedicated enable pins: A2 (PIN_LANE1) enables the left‑lane 74HC238, and A3 (PIN_LANE2) enables the right‑lane 74HC238.

---

### Sensor Handling

The *sensors* module encapsulates finish sensor handling:

* A *SensorConfig* structure defines *leftPin* (A1), *rightPin* (A0), *activeHigh* polarity, and time filters (*minRaceTimeUs* and *maxRaceTimeUs*). Pin assignments can be changed without touching the rest of the code.
* *setupSensors*() configures pin modes but does not attach interrupts. Interrupts are attached in *armSensors*() and detached in *disarmSensors*() to minimise spurious triggers when idle.
* When a race starts, *armSensors*(*startMicros*) records the absolute start time and attaches interrupts on the correct edge. The ISRs record the elapsed time (*micros() - start*) only once per lane; triggers within *minRaceTimeUs* are ignored to prevent bounce false‑starts.
* Volatile flags *leftFinished* / *rightFinished* are exposed for polled access. Helper functions *isLeftFinished*(), *isRightFinished*(), *getLeftTimeUs*(), and *getRightTimeUs*() provide safe access to the recorded times.

---

### Race Logic

Race state transitions are driven externally via messages from the start controller. The finish controller participates actively during *RACE_COUNTDOWN*, *RACE_RACING*, and *RACE_COMPLETE*.

**RACE_COUNTDOWN** – *rx.RaceStart* is cleared at IDLE entry via `rx.clearHeatEvents()`, so it is already false when COUNTDOWN is entered. When *rx.RaceStart* arrives from the start controller, *micros*() is captured as `timingInputs.startUs` and *armSensors*() is called. The state transitions to *RACE_RACING* when the start controller sends the matching state change.

**RACE_RACING** – On entry, recording flags and times are reset. Sensors are armed if not already armed from COUNTDOWN. Each loop iteration:

1. *handleSensors*() polls *isLeftFinished*() / *isRightFinished*() and records finish times. If *maxRaceTimeUs* elapses before a sensor triggers, the max time is recorded for that lane.
2. *handleRxReaction*() reads `rx.LeftReactionTime`, `rx.RightReactionTime`, `rx.LeftFoul`, `rx.RightFoul` from the *SerialRxState* struct (updated by *rxSerial*()) and stores them in `heatResult` (`HeatResults`). Reaction times are stored as `uint32_t`; companion flags `rx.LeftReactionValid` / `rx.RightReactionValid` indicate whether a value has been received this race (both cleared to `false` at IDLE entry via `rx.clearHeatEvents()`).
3. Once both lanes are recorded, *stm.target* is set to *RACE_COMPLETE* and the state machine transitions.

**RACE_COMPLETE** – On entry:

1. *computeHeatResults*(`heatResult`, `timingInputs`) completes the `HeatResults` struct in-place. `heatResult.left.foul` and `heatResult.left.reactionTimeUs` (and right) are already populated from RACING via *handleRxReaction*(). The function adds the remaining derived fields:
   * `raceTimeUs` — copied from `timingInputs` (ISR-captured elapsed time)
   * `carTimeUs = raceTimeUs + (foul ? +1 : -1) * reactionTimeUs` — a foul adds reaction time; a clean start subtracts it
   * `winner` — false if the lane fouled; otherwise true if the opponent fouled or this lane's `carTimeUs` is lower
2. *displayCarTimes*() calls `updateDisplay(carTimeUs, lane)` for each lane. Foul lanes are blanked. If the mode is not *MODE_GATEDROP*, *needReact* is set so reaction times are shown on the next display advance.
3. `pending.queue(MSG_WINNER)` enqueues *MSG_WINNER* with a bitmask (`bit 0 = left, bit 1 = right, bit 2 = tie`).

On subsequent *MSG_DISP_ADVANCE* events (triggered by the operator pressing Start on the start controller):

* If *needReact* is true: *displayReactionTimes*() is called and *needReact* is cleared.
* Otherwise: the state machine targets *RACE_IDLE* and calls *txRaceState(RACE_IDLE)* to coordinate return to idle with the start controller.

---

### State and Mode Variables

State is managed by a local `stateMachine` struct (`stm`) within *finishController.cpp*. It holds *current*, *target*, *entry*, and *exit* flags. The allowed transition table is:

| From \ To    | IDLE | STAGING | COUNTDOWN | RACING | COMPLETE | TEST |
|--------------|:----:|:-------:|:---------:|:------:|:--------:|:----:|
| IDLE         |      | ✓       |           |        |          | ✓    |
| STAGING      |      |         | ✓         |        |          |      |
| COUNTDOWN    |      |         |           | ✓      |          |      |
| RACING       |      |         |           |        | ✓        |      |
| COMPLETE     | ✓    |         |           |        |          |      |
| TEST         | ✓    |         |           |        |          |      |

The current race mode is held in *currentMode* (local to *finishController.cpp*) and kept in sync with `rx.Mode` from the start controller.

---

### Display Design

The firmware drives two 5‑digit seven‑segment displays via direct GPIO. `updateDisplay(uint32_t timeUs, Lane lane)` performs:

1. Round time to the nearest millisecond: `tMs = (timeUs + 500) / 1000`. Clamp at 99,998 ms to prevent a 6‑digit overflow.
2. Extract five digits: tens, ones, tenths, hundredths, thousandths (e.g. 1,234 ms → 0, 1, 2, 3, 4).
3. Select the target lane by driving PIN_LANE1 (A2) or PIN_LANE2 (A3) LOW (active‑low enable on the 74HC238).
4. For each digit, call `writeDigit(idx, val, showDecimal)`:
   * Set the decimal‑point pin (D9) HIGH only for digit index 1 (between ones and tenths).
   * Drive address pins D2–D4 with the digit index (A0/A1/A2 on the demux).
   * Drive data pins D5–D8 with the 4‑bit BCD value (AD0–AD3 on the MC14543B).
   * Wait 30 µs for the MC14543B latch to settle.
5. Disable both lane pins HIGH after all digits are written.

`clearDisplay(Lane lane)` follows the same lane‑select sequence but writes BCD `1111` (0xF) to all five positions, which the MC14543B interprets as a blank.

#### Pin Summary

| Signal       | Pin | Description                          |
|--------------|-----|--------------------------------------|
| PIN_BCD_MUX_A | D2 | Digit address bit 0 (74HC238 A0)     |
| PIN_BCD_MUX_B | D3 | Digit address bit 1 (74HC238 A1)     |
| PIN_BCD_MUX_C | D4 | Digit address bit 2 (74HC238 A2)     |
| PIN_AD0      | D5  | BCD data bit 0 (MC14543B AD0)        |
| PIN_AD1      | D6  | BCD data bit 1 (MC14543B AD1)        |
| PIN_AD2      | D7  | BCD data bit 2 (MC14543B AD2)        |
| PIN_AD3      | D8  | BCD data bit 3 (MC14543B AD3)        |
| PIN_DECIMAL  | D9  | Decimal point (active HIGH)          |
| PIN_LANE1    | A2  | Left‑lane 74HC238 enable (active LOW)|
| PIN_LANE2    | A3  | Right‑lane 74HC238 enable (active LOW)|

---

### Communication with Start Controller

The finish controller communicates with the start controller over a serial connection handled by *serialComm*. The protocol defines **12 message types** (MSG_NULL through MSG_DISP_ADVANCE). Key elements:

* **`rxSerial()`** – Called on every loop iteration. Parses incoming messages and updates the global `SerialRxState rx` struct. Fields used by the finish controller include `rx.State`, `rx.Mode`, `rx.RaceStart`, `rx.LeftFoul`, `rx.RightFoul`, `rx.LeftReactionTime`, `rx.RightReactionTime`, `rx.LeftReactionValid`, `rx.RightReactionValid`, and `rx.DisplayAdvanceFlag`.
* **`txWinner(uint8_t winnerMask)`** – Sends MSG_WINNER to the start controller. Bit 0 = left wins, bit 1 = right wins, bit 2 = tie.
* **`txRaceState(raceState newState)`** – Requests a coordinated state change. Used by the finish controller to initiate the return to RACE_IDLE from RACE_COMPLETE. Transitions are committed only when the start controller sends an ACK; TX_TIMEOUT or TX_FAILED causes the attempt to be abandoned.
* **`txService()`** – Drives the TX FIFO queue; must be called once per main loop. Callers enqueue with `tx*()` functions and poll outcome with `txStatusOf(serialMsgID)`.

All TX functions use a single-in-flight FIFO queue with 3 retries and a 50 ms per-attempt timeout (`txTimeout = 50`). MSG_RACE_START is priority and jumps to the front of the queue.

---

### Communication with Race Manager (BLE)

The Nano 33 BLE includes a Nordic BLE radio. The BLE protocol and characteristic layout are not yet defined. When implemented, race results (*carTimeUs*, *raceTimeUs*, *reactionTimeUs*, *foul*, *winner*) should be transmitted once the race completes. Placeholder comments in *finishController.cpp* (e.g. `// notifyBLEMode(currentMode)`) mark integration points.

---

### Unknowns & Future Work

* **Confirm display wiring** – Verify that PIN_LANE1/PIN_LANE2 active‑low logic matches the 74HC238 enable wiring on your shield. Confirm that BCD 0xF blanks the display on your MC14543B revision.
* **Implement BLE** – Define a BLE GATT service to transmit race results and, if desired, receive configuration commands from the race manager. Ensure BLE callbacks do not block timing‑critical sections.
* **RFID car ID** – Car identification is deferred to the `feature-rfid` branch (P3-1, P3-2). `LaneResult` will need a `carID` field once RFID is implemented.

---

### Recommendations & Next Steps

1. **Verify hardware** with a logic analyser or oscilloscope. Confirm sensors trigger correctly, the min‑time filter suppresses false triggers, and the displays show stable digits without flicker.
2. **Simulate races** by manually breaking the beams and sending artificial reaction times via the start controller. Validate car‑time arithmetic (addition for fouls, subtraction otherwise).
3. **Refine serial protocols** – Ensure the start controller sends foul and reaction messages promptly and that the finish controller acknowledges them. Handle timeouts gracefully.
4. **Plan BLE integration** – Sketch a BLE data format for race results and test with a smartphone before integrating into the main firmware.
