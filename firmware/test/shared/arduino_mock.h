// Minimal Arduino.h stub for native (PC) unit tests.
// Provides only what the tested functions actually reference.
#pragma once
#include <stdint.h>
#include <string.h>
typedef bool boolean;
static inline uint32_t millis() { return 0; }
static inline uint32_t micros() { return 0; }
static inline void delay(uint32_t) {}
#define HIGH 1
#define LOW  0
#define A0 14
#define A1 15
