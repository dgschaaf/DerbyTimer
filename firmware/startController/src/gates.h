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
void updateGates();          // call each tick in STAGING: drives return to completion
void dropGate(Lane lane);    // idempotent: no-op if gate already down
bool isLaneUp(Lane lane);    // is this lane's gate currently held up?
bool areLanesReady();        // return sequence complete and both gates up

#endif  // GATES_H
