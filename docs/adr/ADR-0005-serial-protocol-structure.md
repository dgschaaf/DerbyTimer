# ADR-0002: Serial protocol structure

**Status:** Accepted
**Date:** 2026-05-21

## Context

The two controllers communicate over a single UART at 115200 baud. Several structural decisions were made during initial development that are not otherwise visible in the code.

## Decisions

### Message framing

Every message is a 1-byte message ID followed by a fixed-length payload. Payload length is implied by the message type — there is no length prefix, no checksum, and no end-of-frame marker. The receiver calls `getExpectedPayloadLength()` to know how many bytes to wait for before processing.

Rationale: the message set is small, statically defined, and both ends share the same compiled enum. A dynamic length field or checksum would add complexity with no practical gain given the controlled environment (direct UART, no routing).

### ACK/NACK are immediate fire-and-forget

`txAck()` and `txNack()` write bytes directly to Serial and return. They are never queued. Any message that is not MSG_ACK or MSG_NACK expects exactly one ACK from the receiver.

Rationale: ACK/NACK are responses, not initiators. Queuing them would introduce ordering problems — the receiver has already committed the message and is waiting; any delay in the ACK risks a spurious timeout and retry on the sender side.

### Baud rate: 115200

The baud rate was chosen to keep MSG_RACE_START latency within the timing budget of the race start signal. At 115200 baud a 2-byte message takes ~175 µs on the wire; a full ACK round trip is under 500 µs. Slower baud rates were ruled out because MSG_RACE_START triggers the finish controller's race clock — any wire delay adds directly to timing error.

### serialComm.h does not include raceTypes.h for race state/mode values

`rxSerial()` populates `rx.State` and `rx.Mode` as raw `uint8_t`. Callers cast to `raceState`/`raceMode` from `raceTypes.h`. This keeps serialComm independent of the race domain model — it is a transport layer, not a domain layer.

Exception: `Lane` (from `raceTypes.h`) is used in the `txReactionTime()` signature because it is a hardware-addressing type, not an interpreted domain value, and using it prevents the left/right swap bug that arises with a raw bool.

## Consequences

- Adding a new message requires updating `serialMsgID`, `getExpectedPayloadLength()`, the RX switch, and a new `tx*()` function. No framing or length-prefix code needs to change.
- The "no checksum" choice means a corrupted byte causes a NACK or a parse error; the retry mechanism handles recovery. Persistent corruption triggers `criticalTxError`.
- Future high-throughput use cases (e.g. streaming sensor data) would require revisiting the protocol — the current design assumes a small, event-driven message set.
