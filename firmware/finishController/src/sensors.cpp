#include <Arduino.h>
#include "raceTypes.h"
#include "sensors.h"

// Global configuration.  Adjust leftPin/rightPin for your board.
const SensorConfig config = {
  .leftPin       = A1,         // sensor pin for left lane
  .rightPin      = A0,         // sensor pin for right lane
  .activeHigh    = true,      // sensors pull the pin high when the beam is broken
  .minRaceTimeUs = 500000,    // ignore triggers within 0.5 seconds of race start
  .maxRaceTimeUs = 10000000   // auto complete race after 10 seconds
};

// Internal state (volatile because accessed from ISR). Per-lane values are
// indexed by Lane (LANE_LEFT = 0, LANE_RIGHT = 1).
static volatile uint32_t t0_us          = 0;
static volatile uint32_t finishTime[2]  = {0, 0};
static volatile bool     latched[2]     = {false, false};
static volatile bool     armed          = false;

// Exposed finish flags (declared extern in sensors.h).
volatile bool leftFinished  = false;
volatile bool rightFinished = false;

// Forward declarations of ISRs.
static void leftSensorISR();
static void rightSensorISR();

// The single copy of the beam-break capture rules, shared by both lanes:
// record the finish only if armed, not already latched, and past the
// minimum race time (which rejects the power-up transient). nowUs is passed
// in -- the ISRs supply micros(), tests supply a controlled clock -- so the
// rules can be exercised on the desktop even though an ISR never can.
static void onBeamBreak(Lane lane, uint32_t nowUs) {
    if (!armed || latched[lane]) return;

    uint32_t elapsed = nowUs - t0_us;
    if (elapsed > config.minRaceTimeUs) {
        finishTime[lane] = elapsed;
        latched[lane]    = true;
        if (lane == LANE_LEFT) leftFinished  = true;
        else                   rightFinished = true;
    }
}

void setupSensors() {
    // Configure pins only.  Interrupts are attached in armSensors().
    pinMode(config.leftPin, INPUT);
    pinMode(config.rightPin, INPUT);
}

void armSensors(uint32_t raceStartMicros) {
    // Reset state and record start time.
    noInterrupts();
    t0_us               = raceStartMicros;
    finishTime[LANE_LEFT]  = 0;
    finishTime[LANE_RIGHT] = 0;
    latched[LANE_LEFT]     = false;
    latched[LANE_RIGHT]    = false;
    leftFinished        = false;
    rightFinished       = false;
    armed               = true;
    interrupts();

    // Attach interrupts on the configured edge.
    PinStatus edge = config.activeHigh ? RISING : FALLING;
    attachInterrupt(digitalPinToInterrupt(config.leftPin),  leftSensorISR,  edge);
    attachInterrupt(digitalPinToInterrupt(config.rightPin), rightSensorISR, edge);
}

void disarmSensors() {
    // Detach interrupts to stop ISRs from firing.
    detachInterrupt(digitalPinToInterrupt(config.leftPin));
    detachInterrupt(digitalPinToInterrupt(config.rightPin));

    noInterrupts();
    armed               = false;
    latched[LANE_LEFT]  = false;
    latched[LANE_RIGHT] = false;
    leftFinished        = false;
    rightFinished       = false;
    interrupts();
}

uint32_t getLeftTimeUs() {
    noInterrupts();
    uint32_t t = finishTime[LANE_LEFT];
    interrupts();
    return t;
}

uint32_t getRightTimeUs() {
    noInterrupts();
    uint32_t t = finishTime[LANE_RIGHT];
    interrupts();
    return t;
}

bool isLeftFinished() {
    noInterrupts();
    bool v = leftFinished;
    interrupts();
    return v;
}

bool isRightFinished() {
    noInterrupts();
    bool v = rightFinished;
    interrupts();
    return v;
}

// Interrupt handlers: thin adapters that stamp the current time and defer
// all decisions to onBeamBreak(). Keeping them trivial is deliberate -- an
// ISR is the one context that can never be single-stepped or unit-tested.
static void leftSensorISR()  { onBeamBreak(LANE_LEFT,  micros()); }
static void rightSensorISR() { onBeamBreak(LANE_RIGHT, micros()); }