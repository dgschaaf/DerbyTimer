# Open Questions / Deferred Investigation

## MSG_LEFT_RESULT / MSG_RIGHT_RESULT

**Status:** Deferred — needs investigation before v1.0 close-out

**Location:** `firmware/lib/shared/serialComm.h` (enum), `serialComm.cpp` (RX handler)

**Issue:** `MSG_LEFT_RESULT` and `MSG_RIGHT_RESULT` are declared in the `serialMsgID` enum but have no case in the RX handler switch statement. They are also not used anywhere in the current codebase.

**Questions to answer:**
1. Were these intended for finish → start transmission of final race times (as opposed to reaction times)?
2. Are they needed for v1.0, or are they placeholders for a future Race Manager integration?
3. If not needed now: should they be removed from the enum to keep the protocol clean, or kept as reserved slots?

**Decision needed:** Remove, stub, or implement before first test race.
