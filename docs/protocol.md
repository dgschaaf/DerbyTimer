# DerbyTimer UART Wire Protocol

Byte-level reference for the Start Controller (SC) <-> Finish Controller (FC)
serial protocol. A third party (e.g. the Raspberry Pi Race Manager) should be
able to implement a compatible endpoint from this document alone. The
authoritative implementation is `firmware/lib/shared/serialComm.h/.cpp`.

## Transport

| Property | Value |
| --- | --- |
| Physical | UART, 115200 baud, 8N1 (`serialBaud` in serialComm.h) |
| SC port | `Serial` (only UART on the ATmega328P) |
| FC port | `Serial1` (D0/D1 on the Nano 33 BLE); USB `Serial` is reserved for debug |
| Framing | None -- messages are a bare ID byte followed by a fixed-length payload |
| Integrity | None (no checksum/CRC) -- see "Known limitations" |

## Message format

```
[ ID (1 byte) ][ payload (0-4 bytes, length fixed per ID) ]
```

Payload lengths are defined in `getExpectedPayloadLength()`. Multi-byte
payloads are **little-endian** (both MCUs are little-endian; `memcpy` on the
wire struct).

## Message table

| ID | Name | Payload | Direction | Meaning |
| --- | --- | --- | --- | --- |
| 0 | MSG_NULL | -- | -- | Placeholder, never sent |
| 1 | MSG_ACK | 1: acked msg ID | both | Acknowledge receipt of the named message |
| 2 | MSG_NACK | 1: nacked msg ID | both | Reject the named message (bad value / not allowed); sender retries |
| 3 | MSG_RACE_MODE | 1: raceMode | SC -> FC | Mode change (GATEDROP=0, REACTION=1, PRO=2, DIALIIN=3) |
| 4 | MSG_RACE_STATE | 1: raceState | both | State transition by the initiating controller (see coordination) |
| 5 | MSG_RACE_START | 0 | SC -> FC | GO fired -- FC stamps `micros()` on receipt and arms sensors |
| 6 | MSG_ERROR | 1: errCode | both | SC->FC: critical, FC aborts to IDLE. FC->SC: informational only (ADR-0004) |
| 7 | MSG_LEFT_REACT | 4: uint32 us | SC -> FC | Left lane reaction time in microseconds |
| 8 | MSG_RIGHT_REACT | 4: uint32 us | SC -> FC | Right lane reaction time in microseconds |
| 9 | MSG_FOUL | 1: bitmask | SC -> FC | bit0 = left foul, bit1 = right foul (sent at RACING entry) |
| 10 | MSG_WINNER | 1: bitmask | FC -> SC | bit0 leftWin, bit1 rightWin, bit2 tie, bit3 noResult |
| 11 | MSG_DISP_ADVANCE | 0 | SC -> FC | Operator pressed Start in COMPLETE: advance the display |

`MSG_COUNT` (12) is a sentinel; receivers discard any ID >= 12 silently
(likely line noise -- NACKing garbage would flood the wire).

## ACK / retry discipline

- Every queued message expects an `MSG_ACK` carrying its ID. ACK/NACK
  themselves are fire-and-forget (never ACKed).
- The receiver ACKs **immediately on parse**, before acting on the content.
  A semantically rejected value (e.g. disallowed state transition) gets a
  follow-up NACK from the application layer.
- Sender side: one message in flight at a time, FIFO queue, one slot per
  message ID (re-enqueueing a queued ID is a no-op). `MSG_RACE_START` is
  priority and jumps to the front of the queue.
- Timeout: no ACK within **50 ms** (`txTimeout`) -> TX_TIMEOUT (terminal, no
  resend by the engine; callers decide). NACK -> retry, up to **4 total
  transmission attempts** (`maxRetries` = 3 retries after the initial send),
  then TX_FAILED.
- Payloads are captured at enqueue time, so the wire always carries the value
  current when the event happened.

## RX validation

- Reaction times outside `0..maxValidReactionUs` (10 s) are NACKed and
  discarded.
- A partial message (ID seen, payload incomplete) is left in the buffer; if it
  does not complete within `stalePartialTimeoutMs` (100 ms) the ID byte is
  flushed and NACKed so the sender's retry can resync the stream.

## State coordination model

Both controllers run the same `stateMachine` (stateMachine.h) with one
initiator per transition; the follower commits on `MSG_RACE_STATE` receipt
(edge-triggered via the `rx.StateChanged` flag -- never re-process a stale
`rx.State` value):

| Transition | Initiator |
| --- | --- |
| IDLE -> STAGING, STAGING -> IDLE, STAGING -> COUNTDOWN, COUNTDOWN -> RACING | SC |
| RACING -> COMPLETE, COMPLETE -> IDLE | FC |
| any -> IDLE (abort via `forceIdle()`, best-effort MSG_RACE_STATE(IDLE)) | either |

## Typical heat (REACTION mode)

```
SC: MSG_RACE_STATE(STAGING)      -> FC: ACK
SC: MSG_RACE_STATE(COUNTDOWN)    -> FC: ACK
SC: MSG_RACE_START               -> FC: ACK   (FC stamps t0, arms sensors)
SC: MSG_RACE_STATE(RACING)       -> FC: ACK
SC: MSG_FOUL(mask)               -> FC: ACK   (at RACING entry; includes early starts)
SC: MSG_LEFT_REACT(us)           -> FC: ACK   (at car release, or RACING entry if fouled)
SC: MSG_RIGHT_REACT(us)          -> FC: ACK
FC: MSG_RACE_STATE(COMPLETE)     -> SC: ACK   (both lanes finished or DNF)
FC: MSG_WINNER(mask)             -> SC: ACK
SC: MSG_DISP_ADVANCE             -> FC: ACK   (show reaction times)
SC: MSG_DISP_ADVANCE             -> FC: ACK   (done)
FC: MSG_RACE_STATE(IDLE)         -> SC: ACK
```

## Timing model notes

- FC's race clock t0 is stamped when `MSG_RACE_START` is parsed, not when the
  gate physically drops: UART (2 bytes at 115200 ~ 174 us) plus one loop
  iteration of latency. The offset is common to both lanes, so winner
  determination is unaffected; absolute times carry a small constant bias.
- Reaction times are measured on the SC (gate drop to car release) and carry
  no transport latency.

## Known limitations / planned evolution

- **No framing or checksum**: a corrupted byte can desync the stream until the
  stale-partial flush recovers it; a corrupted payload byte that stays in
  range is undetectable. Deferred decision (2026-06): add `[SOF][len][CRC-8]`
  framing plus a version handshake on the feature-raceManager branch, before
  any third endpoint joins the wire.
- **Extending the protocol**: adding a new message ID is backward-safe (old
  firmware discards unknown IDs). Changing an existing payload length is NOT
  -- old receivers will misparse the stream. Always allocate a new ID instead.
