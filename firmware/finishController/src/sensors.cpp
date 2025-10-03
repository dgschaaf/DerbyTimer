#include "sensors.h"

// Global configuration.  Adjust leftPin/rightPin for your board.
const SensorConfig config = {
  .leftPin       = 4,         // sensor pin for left lane
  .rightPin      = 5,         // sensor pin for right lane
  .activeHigh    = true,      // sensors pull the pin high when the beam is broken
  .minRaceTimeUs = 500000,    // ignore triggers within 0.5 seconds of race start
  .maxRaceTimeUs = 10000000   // auto complete race after 10 seconds
};

// Internal state (volatile because accessed from ISR).
static volatile uint32_t t0_us          = 0;
static volatile uint32_t leftFinishTime = 0;
static volatile uint32_t rightFinishTime= 0;
static volatile bool leftLatched        = false;
static volatile bool rightLatched       = false;
static volatile bool armed              = false;

// Exposed finish flags (declared extern in sensors.h).
volatile bool leftFinished  = false;
volatile bool rightFinished = false;

// Forward declarations of ISRs.
static void leftSensorISR();
static void rightSensorISR();

void setupSensors() {
    // Configure pins only.  Interrupts are attached in armSensors().
    pinMode(config.leftPin, INPUT);
    pinMode(config.rightPin, INPUT);
}

void armSensors(uint32_t raceStartMicros) {
    // Reset state and record start time.
    noInterrupts();
    t0_us          = raceStartMicros;
    leftFinishTime = 0;
    rightFinishTime= 0;
    leftLatched    = false;
    rightLatched   = false;
    leftFinished   = false;
    rightFinished  = false;
    armed          = true;
    interrupts();

    // Attach interrupts on the configured edge.
    int edge = config.activeHigh ? RISING : FALLING;
    attachInterrupt(digitalPinToInterrupt(config.leftPin),  leftSensorISR,  edge);
    attachInterrupt(digitalPinToInterrupt(config.rightPin), rightSensorISR, edge);
}

void disarmSensors() {
    // Detach interrupts to stop ISRs from firing.
    detachInterrupt(digitalPinToInterrupt(config.leftPin));
    detachInterrupt(digitalPinToInterrupt(config.rightPin));

    noInterrupts();
    armed         = false;
    leftLatched   = false;
    rightLatched  = false;
    leftFinished  = false;
    rightFinished = false;
    interrupts();
}

uint32_t getLeftTimeUs() {
    noInterrupts();
    uint32_t t = leftFinishTime;
    interrupts();
    return t;
}

uint32_t getRightTimeUs() {
    noInterrupts();
    uint32_t t = rightFinishTime;
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

// ISR for the left lane sensor.  Records the elapsed time only if armed,
// the finish hasn't already been latched, and the elapsed time exceeds
// minRaceTimeUs.  Updates leftFinished to true when valid.
static void leftSensorISR() {
    if (!armed || leftLatched) return;

    // Compute elapsed time relative to start.
    uint32_t elapsed = micros() - t0_us;
    if (elapsed > config.minRaceTimeUs) {
        leftFinishTime = elapsed;
        leftLatched    = true;
        leftFinished   = true;
    }
}

// ISR for the right lane sensor.
static void rightSensorISR() {
    if (!armed || rightLatched) return;

    uint32_t elapsed = micros() - t0_us;
    if (elapsed > config.minRaceTimeUs) {
        rightFinishTime = elapsed;
        rightLatched    = true;
        rightFinished   = true;
    }
}