#ifndef SENSORS_H
#define SENSORS_H

// Configuration for the finish sensors.  Users may change the pin numbers,
// edge polarity and time filters without modifying the rest of the code.
struct SensorConfig {
  uint8_t leftPin;
  uint8_t rightPin;
  bool activeHigh;
  uint32_t minRaceTimeUs;
  uint32_t maxRaceTimeUs; // maximum race duration in microseconds before auto complete
};

//void setupSensors(const SensorConfig& cfg);
void setupSensors();

void armSensors(uint32_t raceStartMicros);
void disarmSensors();

uint32_t getLeftTimeUs();
uint32_t getRightTimeUs();

// Query finish flags for each lane.
bool isLeftFinished();
bool isRightFinished();

extern volatile bool leftFinished;
extern volatile bool rightFinished;

extern const SensorConfig config;

#endif