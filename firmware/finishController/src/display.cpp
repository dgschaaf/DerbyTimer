#include <Arduino.h>
#include "display.h"

// -------------------------------------------
//  PIN DEFINITIONS  (match your schematic)
// -------------------------------------------
static constexpr uint8_t PIN_BCD_MUX_A = 2;   // D2
static constexpr uint8_t PIN_BCD_MUX_B = 3;   // D3
static constexpr uint8_t PIN_BCD_MUX_C = 4;   // D4

static constexpr uint8_t PIN_AD0 = 5;         // D5
static constexpr uint8_t PIN_AD1 = 6;         // D6
static constexpr uint8_t PIN_AD2 = 7;         // D7
static constexpr uint8_t PIN_AD3 = 8;         // D8

static constexpr uint8_t PIN_DECIMAL = 9;     // D9

// Lane select → goes to 74HC238 EN inputs
static constexpr uint8_t PIN_LANE1 = A2;      // BCD_Lane1 (E2 on U1)
static constexpr uint8_t PIN_LANE2 = A3;      // BCD_Lane2 (E2 on U2)

// Digit index mapping (0–4 -> tens, ones, tenths, hundredths, thousandths)
static constexpr uint8_t NUM_DIGITS = DISPLAY_NUM_DIGITS;

// -------------------------------------------
void setupDisplay() {
    pinMode(PIN_BCD_MUX_A, OUTPUT);
    pinMode(PIN_BCD_MUX_B, OUTPUT);
    pinMode(PIN_BCD_MUX_C, OUTPUT);

    pinMode(PIN_AD0, OUTPUT);
    pinMode(PIN_AD1, OUTPUT);
    pinMode(PIN_AD2, OUTPUT);
    pinMode(PIN_AD3, OUTPUT);

    pinMode(PIN_DECIMAL, OUTPUT);

    pinMode(PIN_LANE1, OUTPUT);
    pinMode(PIN_LANE2, OUTPUT);

    // Default: clear everything
    digitalWrite(PIN_DECIMAL, LOW);
    digitalWrite(PIN_LANE1, HIGH);
    digitalWrite(PIN_LANE2, HIGH);

    // Clear both displays on boot
    updateDisplay(0, LANE_LEFT);
    updateDisplay(0, LANE_RIGHT);
}

// -------------------------------------------
static void writeDigit(uint8_t idx, uint8_t val, bool showDecimal) {

    // Decimal point
    digitalWrite(PIN_DECIMAL, showDecimal ? HIGH : LOW);

    // Digit index → A0/A1/A2
    digitalWrite(PIN_BCD_MUX_A, (idx & 0x01));
    digitalWrite(PIN_BCD_MUX_B, (idx & 0x02));
    digitalWrite(PIN_BCD_MUX_C, (idx & 0x04));

    // BCD lines
    digitalWrite(PIN_AD0, (val & 0x01));
    digitalWrite(PIN_AD1, (val & 0x02));
    digitalWrite(PIN_AD2, (val & 0x04));
    digitalWrite(PIN_AD3, (val & 0x08));

    // The 238 output latches the digit automatically.
    delayMicroseconds(30);  // plenty for MC14543 latch
}

// -------------------------------------------
void updateDisplay(uint32_t timeUs, Lane lane) {

    uint8_t d[NUM_DIGITS];
    extractDisplayDigits(timeUs, d);    // round to ms, clamp, split into BCD digits

    // Lane enable (E2 = HIGH to activate)
    if (lane == LANE_LEFT) {
        digitalWrite(PIN_LANE1, LOW);
        digitalWrite(PIN_LANE2, HIGH);
    } else {
        digitalWrite(PIN_LANE1, HIGH);
        digitalWrite(PIN_LANE2, LOW);
    }

    for (uint8_t i = 0; i < NUM_DIGITS; ++i) {
        bool dec = (i == 1);
        writeDigit(i, d[i], dec);
    }

    // Disable both lanes afterward
    digitalWrite(PIN_LANE1, HIGH);
    digitalWrite(PIN_LANE2, HIGH);
}

static void writeBlankDigit(uint8_t idx)
{
    // Disable decimal
    digitalWrite(PIN_DECIMAL, LOW);

    // Select digit index
    digitalWrite(PIN_BCD_MUX_A, (idx & 0x01));
    digitalWrite(PIN_BCD_MUX_B, (idx & 0x02));
    digitalWrite(PIN_BCD_MUX_C, (idx & 0x04));

    // BCD = 1111 → blank segment output on MC14543
    digitalWrite(PIN_AD0, HIGH);
    digitalWrite(PIN_AD1, HIGH);
    digitalWrite(PIN_AD2, HIGH);
    digitalWrite(PIN_AD3, HIGH);

    delayMicroseconds(30);
}

void clearDisplay(Lane lane)
{
    // ACTIVE LOW enable logic
    if (lane == LANE_LEFT) {
        digitalWrite(PIN_LANE1, LOW);
        digitalWrite(PIN_LANE2, HIGH);
    } else {
        digitalWrite(PIN_LANE1, HIGH);
        digitalWrite(PIN_LANE2, LOW);
    }

    for (uint8_t i = 0; i < NUM_DIGITS; i++) {
        writeBlankDigit(i);
    }

    // Disable both
    digitalWrite(PIN_LANE1, HIGH);
    digitalWrite(PIN_LANE2, HIGH);
}