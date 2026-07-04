#ifndef GATES_H
#define GATES_H

#include <Arduino.h>
#include "raceTypes.h"

/**
 * Gates module — controls the left/right electromagnet gates and return solenoid.
 *
 * Left gate D4, right gate D7, return solenoid D6.
 * Energizing an electromagnet holds the gate up; de-energizing lets the spring drop it.
 * The solenoid briefly pushes both gates up into the magnets during the return sequence.
 */

void setupGates();
void returnGates();          // one-shot: begin gate return sequence
void dropGate(Lane lane);    // idempotent: no-op if gate already down

// Call each tick in STAGING: ends the return pulse once returnWaitMs elapses.
// The main loop calls the zero-arg form (current time = millis()); the
// time-injected form lets tests drive the sequence on a deterministic clock.
void updateGates(unsigned long now);
inline void updateGates() { updateGates(millis()); }
bool isLaneUp(Lane lane);    // is this lane's gate currently held up?
bool areLanesReady();        // return sequence complete and both gates up

#endif  // GATES_H
