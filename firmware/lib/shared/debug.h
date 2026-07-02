// firmware/lib/shared/debug.h
// Compile-time serial debug logging for DerbyTimer firmware.
//
// USAGE: Include this file after Arduino.h in any .cpp that needs debug output.
//   The DERBY_DEBUG flag is set via build flag, not in source:
//   arduino-cli ... --build-property "build.extra_flags=-DDERBY_DEBUG"
//   (See "Compile SC Debug" / "Compile FC Debug" tasks in .vscode/tasks.json)
//
// CONSTRAINT: On the SC (ATmega328P, single UART) Serial is shared with the
//   FC<->SC wire protocol -- do not run an SC debug build with the FC
//   connected, the debug text corrupts the protocol stream. The FC is safe:
//   its protocol runs on Serial1 (P2-37), so DERBY_DEBUG output on USB Serial
//   coexists with live racing. For SC-side protocol observation use
//   tools/uart_monitor.py (L3b).
//
// ZERO OVERHEAD: When DERBY_DEBUG is not defined, all macros expand to
//   no-ops -- no RAM, no flash, no runtime cost in the production build.
#pragma once
#ifdef DERBY_DEBUG
  #define DBG(msg)       Serial.println(F(msg))
  #define DBG2(lbl, val) do { Serial.print(F(lbl)); Serial.println(val); } while(0)
#else
  #define DBG(msg)       ((void)0)
  #define DBG2(lbl, val) ((void)0)
#endif
