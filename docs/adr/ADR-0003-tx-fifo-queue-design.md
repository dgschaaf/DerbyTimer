# ADR-0003: TX FIFO queue with single in-flight message

**Status:** Accepted
**Date:** 2026-05-21

## Context

The original TX design duplicated the send/retry/timeout state machine identically inside every `tx*()` function (~7 copies). This created a copy-paste maintenance burden and made retry policy changes a 7-site edit. It also allowed multiple messages to be "in flight" simultaneously (each had independent state), which was hard to reason about.

## Decision

A FIFO queue inside `serialComm.cpp` serializes all outgoing messages: one message is sent at a time, and the next message is not attempted until the current one reaches a terminal status (TX_ACKED, TX_TIMEOUT, or TX_FAILED). The retry state machine is implemented once in the private `txDrive()` function.

### Public API contract

**Enqueueing:** Each `tx*()` function returns `bool`:
- `true` -- message was newly enqueued (slot was idle or in a terminal state)
- `false` -- message was not enqueued (already in flight TX_SENT, or already waiting in queue)

The bool return allows callers to detect a duplicate enqueue attempt. Callers that don't care about the outcome can ignore the return value.

**Querying:** `txStatusOf(serialMsgID id)` returns the current status of any message slot. Callers poll this each loop to detect terminal status and act accordingly (commit a state change, flag an error, etc.).

**Driving:** `txService()` must be called once per main loop to advance the queue. It sends the front-of-queue message on first call, tracks the timeout window on subsequent calls, and dequeues on terminal status.

**Reset:** `resetTxState()` is internal. A new enqueue automatically resets the tracker slot -- callers never call it directly.

### Priority

MSG_RACE_START is the only priority message. When enqueued, it jumps to the front of the queue. This guarantees minimal latency for the race start signal regardless of what else is queued. No other message has priority status -- MSG_RACE_STATE transitions occur on human timescales where queue position is irrelevant.

### Retry policy

- NACK triggers an automatic retry (up to `maxRetries = 3`), then TX_FAILED.
- Timeout (no ACK within `txTimeout = 50 ms`) -> TX_TIMEOUT (terminal, no automatic retry).
- Callers that want to retry after a timeout must re-enqueue the message explicitly.

### Payload capture

Payload bytes are captured into `TxTracker.payload[4]` at enqueue time. The public `tx*()` wrappers own payload construction (type casting, bitmask building, memcpy). `txDrive()` reads the captured bytes and calls `sendMessage()` -- it has no knowledge of message semantics.

## Consequences

- Adding a new message type requires only a new `tx*()` wrapper (3-5 lines) plus entries in the message ID enum and `getExpectedPayloadLength()`. The retry engine needs no changes.
- The single-in-flight constraint means a stuck message (e.g. repeated NACKs) blocks the queue until it reaches TX_FAILED. The `criticalTxError` path in both controllers handles this for messages where failure is unrecoverable.
- Maximum payload size is 4 bytes (`sizeof(uint32_t)`). If a future message requires a larger payload, `TxTracker.payload` must be widened -- this is an internal change only.
- The RFID UID message (planned) will use a longer payload; if the UID exceeds 4 bytes the payload field must be widened before that message is added.

## Amendment (2026-07): txResend()

`txResend(id)` re-enqueues a message from the payload bytes captured in its tracker slot at the original enqueue -- the payload-capture mechanism above is what makes a resend possible without the caller rebuilding (or retaining) the payload. It is permitted only from the failure statuses (TX_TIMEOUT / TX_FAILED); a message in flight, queued-but-unsent, never sent, or already ACKED is rejected. If the failed entry has not yet been dequeued (a caller checking outcomes in the same loop pass the failure was marked), it is rearmed in place with its retry counter reset -- a plain re-enqueue in that window would be rejected as a duplicate. MSG_RACE_START keeps its front-of-queue priority on resend.

This amends the retry policy's "callers that want to retry after a timeout must re-enqueue the message explicitly": explicit retry is now expressed with `txResend()`, so retry POLICY can live outside the transport -- a policy layer says "send it again" without knowing how the bytes were built.
