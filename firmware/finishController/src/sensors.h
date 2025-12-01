#ifndef SENSORS_H
#define SENSORS_H

/**
 * @brief Configuration for the finish-line sensors.
 *
 * Users may change pin numbers and thresholds without
 * touching the rest of the code.
 */
struct SensorConfig {
  uint8_t leftPin;
  uint8_t rightPin;
  bool activeHigh;
  uint32_t minRaceTimeUs;
  uint32_t maxRaceTimeUs; // maximum race duration in microseconds before auto complete
};

// Global configuration instance (defined in sensors.cpp)
extern const SensorConfig config;

//Setup/teardown
void setupSensors();
void armSensors(uint32_t raceStartMicros);
void disarmSensors();

// Query times (us since race start)
uint32_t getLeftTimeUs();
uint32_t getRightTimeUs();

// Query completion flags
bool isLeftFinished();
bool isRightFinished();

// Raw ISR-set flags (volatile)
extern volatile bool leftFinished;
extern volatile bool rightFinished;

#endif // SENSORS_H