#ifndef GATES_H
#define GATES_H

struct gateStatusInfo {
	bool returnActive;
	bool leftUp;
	bool rightUp;
	unsigned long returnTime;
	uint16_t waitTime;
};

extern gateStatusInfo gateStatus;

void setupGates();
void dropGate(byte gatePin);
void returnGates();

#endif
