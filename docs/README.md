# DerbyTimer Documentation Guide

Map of everything under `docs/`. Each document opens with a line stating what
it is and what is authoritative for its subject (usually the firmware source).
The domain glossary -- the project's shared vocabulary for heats, runs, fouls,
and the rest -- lives at the repo root in [CONTEXT.md](../CONTEXT.md).

## Design references

| Document | What it is | Read it when |
| --- | --- | --- |
| [startController.md](startController.md) | As-built design of the Start Controller (gates, lights, buttons, countdown, reaction timing) | Working on `firmware/startController/` |
| [finishController.md](finishController.md) | As-built design of the Finish Controller (sensors, timing math, displays, winner logic) | Working on `firmware/finishController/` |
| [protocol.md](protocol.md) | Byte-level UART wire protocol between the two controllers | Implementing or debugging anything that touches the serial link |
| [raceManager.md](raceManager.md) | Planning baseline for the future Raspberry Pi Race Manager (BLE, roster, brackets) -- not yet implemented | Designing the raceManager feature branch |

## Testing and operations

| Document | What it is | Read it when |
| --- | --- | --- |
| [testing.md](testing.md) | Layer map of the test strategy (L1 static analysis through L5 debug mode) | Deciding which test to run, or adding tests |
| [bench-test-protocol.md](bench-test-protocol.md) | Cold-start bench procedure for two-board integration, with FTDI tap wiring and a symptom table | Bringing up or debugging real hardware |
| [race-test-codes.md](race-test-codes.md) | Printable failure-code sheet for the built-in RACE_TEST self-test | At the track; print and store with the hardware |
| [shipping-checklist.md](shipping-checklist.md) | v1.0 verification checklist from compile gate to race-day readiness | Preparing for an event |
| [test-log.md](test-log.md) | Append-only record of test runs and results | Checking what was last verified, and when |

## Decision records ([adr/](adr/))

Architecture Decision Records: the reasoning behind choices the code cannot
explain by itself. Add a new ADR when a decision would otherwise get
re-litigated.

| ADR | Decision |
| --- | --- |
| [ADR-0001](adr/ADR-0001-pure-computation-stays-in-owning-controller.md) | Pure computation stays in its owning controller file; file boundaries map to hardware interfaces |
| [ADR-0002](adr/ADR-0002-foul-derived-from-timestamps.md) | Foul status is derived from timestamps, not stored as a flag |
| [ADR-0003](adr/ADR-0003-tx-fifo-queue-design.md) | TX FIFO queue with a single in-flight message |
| [ADR-0004](adr/ADR-0004-heat-abort-criteria.md) | Heat abort criteria and forceIdle semantics |
| [ADR-0005](adr/ADR-0005-serial-protocol-structure.md) | Serial protocol structure: framing, ACK/NACK discipline, baud choice |

## Diagrams ([diagrams/](diagrams/))

Sources only -- no rendered images are committed. `.puml` files need a
PlantUML viewer (VS Code: the "PlantUML" extension); `.drawio` files open at
diagrams.net or with the "Draw.io Integration" VS Code extension.

| Diagram | Shows |
| --- | --- |
| [stm_startController.puml](diagrams/stm_startController.puml) | Start Controller state machine |
| [stm_finishController.puml](diagrams/stm_finishController.puml) | Finish Controller state machine |
| [stm_raceManager.puml](diagrams/stm_raceManager.puml) | Planned Race Manager state machine |
| [seq_stateMachine.puml](diagrams/seq_stateMachine.puml) | Coordinated state-transition protocol (selfTransition / rxTransition) |
| [seq_idle.puml](diagrams/seq_idle.puml), [seq_staging.puml](diagrams/seq_staging.puml), [seq_countdown.puml](diagrams/seq_countdown.puml), [seq_racing.puml](diagrams/seq_racing.puml), [seq_complete.puml](diagrams/seq_complete.puml) | SC-FC message sequence in each race state |
| [activity_countdown.puml](diagrams/activity_countdown.puml) | RACE_COUNTDOWN activity flow on the Start Controller |
| [timing_reactionTime.puml](diagrams/timing_reactionTime.puml) | Reaction-time measurement model (Reaction mode) |
| [component_modules.puml](diagrams/component_modules.puml) | Firmware module dependencies |
| [class_module.puml](diagrams/class_module.puml) | Start Controller class/struct structure |
| [physical_interconnect.drawio](diagrams/physical_interconnect.drawio) | Board-to-board physical wiring |
| [power_distribution.drawio](diagrams/power_distribution.drawio) | Power rails and distribution |

`Track Architecture.pdf` (with its Visio source `Track Architecture.vsdx`) is
the top-level physical track layout.

## Hardware spec sheets ([hardware_spec_sheets/](hardware_spec_sheets/))

Vendor datasheets, pinouts, and schematics for the Arduino Nano (A000005) and
Nano 33 BLE (ABX00071), plus the legacy commercial track design files the
project started from. Reference material only; nothing here is
project-authored.
