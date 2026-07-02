// Mock Arduino.h for native (PC) unit tests.
// Resolves <Arduino.h> includes inside production sources (e.g. serialComm.cpp)
// when compiled in the [env:native] environment (-I firmware/test/shared).
//
// Everything is static so each test binary (one translation unit) gets its own
// instance. Tests drive time via mockMillis/mockMicros and script serial I/O
// via Serial.feed()/Serial.txBuf.
#pragma once
#include <stdint.h>
#include <string.h>
#include <stddef.h>

typedef bool boolean;
typedef uint8_t byte;

#define HIGH 1
#define LOW  0
#define A0 14
#define A1 15
#define A2 16
#define A3 17

// ---- Controllable clocks ----
static uint32_t mockMillis = 0;
static uint32_t mockMicros = 0;
static inline uint32_t millis() { return mockMillis; }
static inline uint32_t micros() { return mockMicros; }
static inline void delay(uint32_t) {}
static inline void delayMicroseconds(uint32_t) {}

// ---- Scriptable serial port ----
// RX side: bytes the code under test will consume (test calls feed()).
// TX side: bytes the code under test wrote (test inspects txBuf/txLen).
struct MockSerial {
    uint8_t rxBuf[256]; int rxHead = 0; int rxTail = 0;
    uint8_t txBuf[256]; int txLen  = 0;

    void   begin(unsigned long) {}
    int    available()          { return rxTail - rxHead; }
    int    peek()               { return (rxHead < rxTail) ? rxBuf[rxHead] : -1; }
    int    read()               { return (rxHead < rxTail) ? rxBuf[rxHead++] : -1; }
    size_t readBytes(uint8_t* dst, size_t n) {
        size_t i = 0;
        while (i < n && rxHead < rxTail) dst[i++] = rxBuf[rxHead++];
        return i;
    }
    size_t write(uint8_t b) {
        if (txLen < (int)sizeof(txBuf)) txBuf[txLen++] = b;
        return 1;
    }
    size_t write(const uint8_t* d, size_t n) {
        for (size_t i = 0; i < n; i++) write(d[i]);
        return n;
    }

    // ---- Test helpers (not part of the Arduino API) ----
    void feed(const uint8_t* d, size_t n) {
        for (size_t i = 0; i < n; i++)
            if (rxTail < (int)sizeof(rxBuf)) rxBuf[rxTail++] = d[i];
    }
    void feedByte(uint8_t b) { feed(&b, 1); }
    void clearTx()           { txLen = 0; }
    void reset()             { rxHead = rxTail = txLen = 0; }
};
static MockSerial Serial;

// serialComm.h declares setupSerialBus(Stream&); natively the mock plays
// the role of Stream (it provides the same read/write surface).
using Stream = MockSerial;
