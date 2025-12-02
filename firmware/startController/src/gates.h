#ifndef GATES_H
#define GATES_H

/**
 * @brief Configuration for the solenoid and electromagnets controlling the gates.
 *
 * Left gate is D4, right gate is D7, return solenoid is D6.+
 * 
 * Return energizes the electromagnets and briefly 
 * energizes the solenoid to push them up
 */

struct gateStatusInfo {
	bool returnActive;
	bool leftUp;
	bool rightUp;
	unsigned long returnTime;
	uint16_t waitTime;
};

// Globalc configuration instance (defined in gates.cpp)
extern byte gateL;
extern byte gateR;
extern byte gateReturn;

extern gateStatusInfo gateStatus;

// Setup/teardown
void setupGates();

// Public API
void dropGate(byte gatePin);
void returnGates();

#endif	// GATES_H
