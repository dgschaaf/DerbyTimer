# RACE_TEST Failure Code Reference

Print this sheet and store it with the timer hardware.

## Entry

**Start Controller:** Hold the MODE button during power-up.
**Finish Controller:** Enters RACE_TEST automatically when SC triggers it.

Both controllers can also be triggered by sending `MSG_RACE_STATE(RACE_TEST)` over UART.

## Failure Codes

| Code | Controller | Phase | Meaning |
| ---- | ---------- | ----- | ------- |
| **101** | SC | Phase 2 - Gates | Left gate did not reach ready state within 600 ms |
| **102** | SC | Phase 2 - Gates | Right gate did not reach ready state within 600 ms |
| **103** | SC | Phase 3 - Buttons | Start button not detected within 5 seconds |
| **104** | SC | Phase 3 - Buttons | Mode button not detected within 5 seconds |
| **105** | SC | Phase 3 - Buttons | Left lane button not detected within 5 seconds |
| **106** | SC | Phase 3 - Buttons | Right lane button not detected within 5 seconds |
| **107** | SC | Phase 0 - FC Comm | FC did not respond to communication ping (non-fatal; test continues) |
| **201** | FC | Phase 2 - Sensors | Left sensor: no beam-break detected within 8 seconds |
| **202** | FC | Phase 2 - Sensors | Right sensor: no beam-break detected within 8 seconds |
| **203** | FC | Phase 3 - SC Comm | SC communication ping not received within 3 seconds |

## SC Result Indicators (lights)

| Result | Pattern |
| ------ | ------- |
| All pass | GO (green) blinks 5x then stays on |
| Any failure | FL + FR (red) blinks N times where N = number of failed codes, pauses 1s, repeats |

## FC Result Indicators (displays)

| Result | Display |
| ------ | ------- |
| All pass | Both lanes show `00.000` |
| Any failure | Both lanes cycle: code (2s), blank (1s), next code (2s), ..., `88.888` end marker (1s), repeat |

Failure codes display as `00.XXX` where XXX is the 3-digit code.
Example: code 107 displays as `00.107`.

## SC Test Sequence

| Phase | What Happens | Observable |
| ----- | ------------ | ---------- |
| 0 | Pings FC over UART, waits 2s for ACK | Blue lights blink 3x (pass) or red blink 1x (code 107) |
| 1 | Chases all 8 lights L-to-R then R-to-L (150ms each); 3x all-on/all-off (500ms each) | Watch for dead LEDs |
| 2 | Returns both gates (hold 600ms), drops left (300ms), drops right (300ms) | Watch gates physically |
| 3 | Prompts each button: START, MODE, LEFT lane, RIGHT lane (5s each) | GO blinks = press a button; brief confirm blink = button detected |
| Result | GO stays green (all pass) or red blinks repeat (failures) | -- |

## FC Test Sequence

| Phase | What Happens | Observable |
| ----- | ------------ | ---------- |
| 1 | Shows `88.888` on both lanes (1s), then counts down `05.000` to `01.000` (500ms each) | Watch for dead display segments |
| 2 | Arms sensors; both lanes count up live | Wave hand in front of each sensor; display freezes at captured time |
| 3 | Waits 3s for SC communication confirmation | Internal only |
| Result | `00.000` (all pass) or failure code cycle | -- |

## Notes

- Light test (SC Phase 1) and display test (FC Phase 1) are visual-only. No failure code is generated -- observe directly.
- Gate test is physical -- watch for gates to rise and drop.
- Button test is interactive -- press each button when the GO light is blinking.
- Sensor test is interactive -- wave your hand in front of each sensor beam after the countdown ends.
- Code 107 is non-fatal: SC continues all remaining test phases even if FC is not responding.
- Sensor minimum trigger time is 500ms after arm; wave your hand after the display starts counting.

## Exiting RACE_TEST

Power-cycle both controllers to exit. Future: remote `MSG_RACE_STATE(IDLE)` from Race Manager.
